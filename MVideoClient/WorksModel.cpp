//
// Created by mpt on 2026/11/10.
//

#include "WorksModel.h"`r`n`r`n#include <QDebug>

WorksModel::WorksModel(QObject* parent) : QAbstractListModel(parent) {}

int WorksModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(_items.size());
}

QVariant WorksModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= _items.size()) {
        return {};
    }
    const auto& item = _items.at(index.row());
    switch (role) {
        case RequestIdRole:
            return item.requestId;
        case TitleRole:
            return item.title;
        case DescriptionRole:
            return item.description;
        case TagsRole:
            return item.tags;
        case VideoIdRole:
            return item.videoId;
        case StateRole:
            return item.state;
        case CoverUrlRole:
            return item.coverUrl;
        default:
            return {};
    }
}

QHash<int, QByteArray> WorksModel::roleNames() const {
    return {
            {RequestIdRole,   "requestId"},
            {TitleRole,       "title"},
            {DescriptionRole, "description"},
            {TagsRole,        "tags"},
            {VideoIdRole,     "videoId"},
            {StateRole,       "workState"},
            {CoverUrlRole,    "coverUrl"},
    };
}

void WorksModel::append(const WorkItem& item) {
    const int row = static_cast<int>(_items.size());
    beginInsertRows(QModelIndex(), row, row);
    _items.append(item);
    endInsertRows();
    emit countChanged();
}

void WorksModel::clear() {
    if (_items.isEmpty()) {
        return;
    }
    beginResetModel();
    _items.clear();
    endResetModel();
    emit countChanged();
}

bool WorksModel::setVideoId(const QString& requestId, qint64 videoId) {
    for (int row = 0; row < _items.size(); ++row) {
        if (_items.at(row).requestId != requestId) {
            continue;
        }
        if (_items.at(row).videoId == videoId) {
            return false;
        }
        _items[row].videoId = videoId;
        const auto changed = index(row, 0);
        emit dataChanged(changed, changed, {VideoIdRole});
        return true;
    }
    return false;
}

bool WorksModel::setState(qint64 videoId, int state) {
    const int row = indexOfVideo(videoId);
    if (row < 0) {
        return false;
    }
    auto& item = _items[row];
    if (item.state == state) {
        return false;
    }
    if (item.state == 4) {
        return false;
    }
    // 失败(4)可以随时写入，其余状态只前进不倒退
    if (state != 4 && item.state > state) {
        return false;
    }
    item.state = state;
    const auto changed = index(row, 0);
    emit dataChanged(changed, changed, {StateRole});
    return true;
}

bool WorksModel::setStateByRequestId(const QString& requestId, int state) {
    if (requestId.isEmpty()) {
        return false;
    }
    for (int row = 0; row < _items.size(); ++row) {
        if (_items.at(row).requestId != requestId) {
            continue;
        }
        if (_items.at(row).state == state) {
            return false;
        }
        _items[row].state = state;
        qDebug() << "[works]" << requestId << "state ->" << state;
        const auto changed = index(row, 0);
        emit dataChanged(changed, changed, {StateRole});
        return true;
    }
    return false;
}

qint64 WorksModel::latestPendingVideoId() const {
    for (int row = static_cast<int>(_items.size()) - 1; row >= 0; --row) {
        if (_items.at(row).state <= 1) {
            return _items.at(row).videoId;
        }
    }
    return 0;
}
QList<qint64> WorksModel::videoIdsWithState(int state) const {
    QList<qint64> ids;
    for (const auto& item : _items) {
        if (item.state == state && item.videoId > 0) {
            ids.append(item.videoId);
        }
    }
    return ids;
}

int WorksModel::indexOfVideo(qint64 videoId) const {
    if (videoId <= 0) {
        return -1;
    }
    for (int row = 0; row < _items.size(); ++row) {
        if (_items.at(row).videoId == videoId) {
            return row;
        }
    }
    return -1;
}