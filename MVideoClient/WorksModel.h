//
// Created by mpt on 2026/11/10.
//

#ifndef MVIDEOCLIENT_WORKSMODEL_H
#define MVIDEOCLIENT_WORKSMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>

struct WorkItem {
    QString requestId;
    QString title;
    QString description;
    QStringList tags;
    qint64 videoId{0};
    int state{0};          // 0 上传中 1 等待校验 2 解码中 3 已发布 4 失败
    int uploadPercent{0};  // 0~100，只在 state 0/1 有意义
    QString coverUrl;
};

class WorksModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        RequestIdRole = Qt::UserRole + 1,
        TitleRole,
        DescriptionRole,
        TagsRole,
        VideoIdRole,
        StateRole,
        CoverUrlRole,
        UploadPercentRole,
    };
    Q_ENUM(Role)

    explicit WorksModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return static_cast<int>(_items.size()); }

    void append(const WorkItem& item);
    void clear();

    bool setVideoId(const QString& requestId, qint64 videoId);
    bool setState(qint64 videoId, int state, const QString& coverUrl = QString());
    bool setStateByRequestId(const QString& requestId, int state);
    bool setUploadProgress(qint64 videoId, int percent);
    [[nodiscard]] qint64 latestPendingVideoId() const;
    [[nodiscard]] QList<qint64> videoIdsWithState(int state) const;

signals:
    void countChanged();

private:
    [[nodiscard]] int indexOfVideo(qint64 videoId) const;

    QList<WorkItem> _items;
};

#endif //MVIDEOCLIENT_WORKSMODEL_H