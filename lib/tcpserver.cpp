/*
 * tcpserver.cpp
 *
 * Copyright (C) 2015 Symeon Huang <hzwhuang@gmail.com>
 *
 * This file is part of the libQtShadowsocks.
 *
 * libQtShadowsocks is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * libQtShadowsocks is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libQtShadowsocks; see the file LICENSE. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "tcpserver.h"
#include "common.h"
#include <QThread>
#include <thread>

using namespace QSS;

TcpServer::TcpServer(const EncryptorPrivate &ep,
                     const int &timeout,
                     const bool &is_local,
                     const bool &auto_ban,
                     const bool &auth,
                     const Address &serverAddress,
                     const Address* _redirect_addr,
                     QObject *parent) :
    QTcpServer(parent),
    isLocal(is_local),
    autoBan(auto_ban),
    auth(auth),
    serverAddress(serverAddress),
    redirect_addr(_redirect_addr),
    timeout(timeout),
    ep(ep),
    workerThreadID(0)
{
    totalWorkers = std::thread::hardware_concurrency();
    if (totalWorkers == 0) {
        totalWorkers = 1;// need at least one working thread
    }
    for (unsigned int i = 0; i < totalWorkers; ++i) {
        QThread *t = new QThread(this);
        threadList.append(t);
    }
}

void TcpServer::setHttpRedirectAddr(const Address *addr) {
    redirect_addr = addr;
}

TcpServer::~TcpServer()
{
    for (auto&& con : conList) {
        con->deleteLater();
    }

    if (isListening()) {
        close();
    }
}

void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *localSocket = new QTcpSocket;
    localSocket->setSocketDescriptor(socketDescriptor);

    if (!isLocal && autoBan && Common::isAddressBanned(localSocket->peerAddress())) {
        emit debug(QString("A banned IP %1 attempted to access this server")
                   .arg(localSocket->peerAddress().toString()));
        localSocket->deleteLater();
        return;
    }

    //timeout * 1000: convert sec to msec
    TcpRelay *con = new TcpRelay(localSocket,
                                 timeout * 1000,
                                 serverAddress,
                                 ep,
                                 isLocal,
                                 autoBan,
                                 auth,
                                 redirect_addr);
    conList.append(con);
    connect(con, &TcpRelay::info, this, &TcpServer::info);
    connect(con, &TcpRelay::debug, this, &TcpServer::debug);
    connect(con, &TcpRelay::bytesRead, this, &TcpServer::bytesRead);
    connect(con, &TcpRelay::bytesSend, this, &TcpServer::bytesSend);
    connect(con, &TcpRelay::latencyAvailable,
            this, &TcpServer::latencyAvailable);
    connect(con, &TcpRelay::finished, this, &TcpServer::onConnectionFinished);
    con->moveToThread(threadList.at(workerThreadID++));
    workerThreadID %= totalWorkers;
}

void TcpServer::onConnectionFinished()
{
    TcpRelay *con = qobject_cast<TcpRelay*>(sender());
    //sometimes the finished signal from TcpRelay gets emitted multiple times
    if (conList.removeOne(con)) {
        con->deleteLater();
    }
}

bool TcpServer::listen(const QHostAddress &address, quint16 port)
{
    bool l = QTcpServer::listen(address, port);
    if (l) {
        for (auto&& thread : threadList) {
            thread->start();
        }
    }
    return l;
}

void TcpServer::close()
{
    for (auto&& thread : threadList) {
        thread->quit();
    }
    for (auto&& thread : threadList) {
        thread->wait();
    }
    QTcpServer::close();
}
