//
// Created by mpt on 2026/11/18.
//

#ifndef MVIDEOCLIENT_REQUESTID_H
#define MVIDEOCLIENT_REQUESTID_H

#include <QString>
#include <QUuid>

inline QString MakeRequestId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

#endif //MVIDEOCLIENT_REQUESTID_H