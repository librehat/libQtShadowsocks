/*
 * client.cpp - source file of Client class
 *
 * Copyright (C) 2014-2015 Symeon Huang <hzwhuang@gmail.com>
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

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "client.h"

Client::Client(QObject *parent) :
    QObject(parent),
    lc(nullptr),
    autoBan(false)
{}

bool Client::readConfig(const QString &file)
{
    QFile c(file);
    if (!c.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QSS::Common::qOut << "can't open config file " << file << endl;
        return false;
    }
    if (!c.isReadable()) {
        QSS::Common::qOut << "config file " << file
                          << " is not readable!" << endl;
        return false;
    }
    QByteArray confArray = c.readAll();
    c.close();

    QJsonDocument confJson = QJsonDocument::fromJson(confArray);
    QJsonObject confObj = confJson.object();
    profile.local_address = confObj["local_address"].toString();
    profile.local_port = confObj["local_port"].toInt();
    profile.method = confObj["method"].toString();
    profile.password = confObj["password"].toString();
    profile.server = confObj["server"].toString();
    profile.server_port = confObj["server_port"].toInt();
    profile.timeout = confObj["timeout"].toInt();
    profile.http_proxy = confObj["http_proxy"].toBool();
    profile.http_redirect = confObj["http_redirect"].toString();
    profile.auth = confObj["auth"].toBool();

    return true;
}

void Client::setup(const QString &remote_addr,
                   const QString &remote_port,
                   const QString &local_addr,
                   const QString &local_port,
                   const QString &password,
                   const QString &method,
                   const QString &timeout,
                   const bool http_proxy,
                   const QString &http_redirect,
                   const bool debug,
                   const bool auth)
{
    profile.server = remote_addr;
    profile.server_port = remote_port.toInt();
    profile.local_address = local_addr;
    profile.local_port = local_port.toInt();
    profile.password = password;
    profile.method = method;
    profile.timeout = timeout.toInt();
    profile.http_proxy = http_proxy;
    profile.http_redirect = http_redirect;
    profile.debug = debug;
    profile.auth = auth;
}

void Client::setAutoBan(bool ban)
{
    autoBan = ban;
}

void Client::setDebug(bool debug)
{
    profile.debug = debug;
}

void Client::setHttpMode(bool http)
{
    profile.http_proxy = http;
}

void Client::setAuth(bool auth)
{
    profile.auth = auth;
}

bool Client::start(bool _server)
{
    if (profile.debug) {
        if (!headerTest()) {
            QSS::Common::qOut << "Header test failed" << endl;
            return false;
        }
    }

    if (lc) {
        lc->deleteLater();
    }
    lc = new QSS::Controller(!_server, autoBan, this);
    connect (lc, &QSS::Controller::info, this, &Client::logHandler);
    if (profile.debug) {
        connect(lc, &QSS::Controller::debug, this, &Client::logHandler);
    }
    lc->setup(profile);

    if (!_server) {
        QSS::Address server(profile.server, profile.server_port);
        QSS::AddressTester *tester =
                new QSS::AddressTester(server.getFirstIP(),
                                       server.getPort());
        connect(tester, &QSS::AddressTester::connectivityTestFinished,
                this, &Client::onConnectivityResultArrived);
        tester->startConnectivityTest(profile.method,
                                      profile.password,
                                      profile.auth);
    }

    return lc->start();
}

bool Client::headerTest()
{
    int length;
    bool unused_auth;
    QHostAddress test_addr("1.2.3.4");
    QHostAddress test_addr_v6("2001:0db8:85a3:0000:0000:8a2e:1010:2020");
    quint16 test_port = 56;
    QSS::Address test_res, test_v6(test_addr_v6, test_port);
    QByteArray packed = QSS::Common::packAddress(test_v6);
    QSS::Common::parseHeader(packed, test_res, length, unused_auth);
    bool success = (test_v6 == test_res);
    if (!success) {
        QSS::Common::qOut << test_v6.toString() << " --> "
                          << test_res.toString() << endl;
    }
    packed = QSS::Common::packAddress(test_addr, test_port);
    QSS::Common::parseHeader(packed, test_res, length, unused_auth);
    bool success2 = ((test_res.getFirstIP() == test_addr)
                 && (test_res.getPort() == test_port));
    if (!success2) {
        QSS::Common::qOut << test_addr.toString().toLocal8Bit()
                          << ":" << test_port << " --> "
                          << test_res.toString() << endl;
    }
    return success & success2;
}

void Client::logHandler(const QString &log)
{
    QSS::Common::qOut << log << endl;
}

QString Client::getMethod() const
{
    return profile.method;
}

void Client::onConnectivityResultArrived(bool c)
{
    if (c) {
        QSS::Common::qOut << "The shadowsocks connection is okay." << endl;
    } else {
        QSS::Common::qOut << "Destination is not reachable. "
                             "Please check your network and firewall settings. "
                             "And make sure the profile is correct."
                          << endl;
    }
    sender()->deleteLater();
}
