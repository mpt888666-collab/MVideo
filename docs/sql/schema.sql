-- MVideo database schema
-- MySQL 8.0+
-- Apply with: mysql -uroot -p < docs/sql/schema.sql

SET NAMES utf8mb4;

CREATE DATABASE IF NOT EXISTS MVideo
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE MVideo;

-- ---------------------------------------------------------------------------
-- Account and profile
-- ---------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS user (
    user_id       BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    account       VARCHAR(64)     NOT NULL,
    phone         VARCHAR(32)     NULL,
    email         VARCHAR(255)    NOT NULL,
    password_hash VARCHAR(255)    NOT NULL,
    created_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    updated_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                                      ON UPDATE CURRENT_TIMESTAMP(3),
    PRIMARY KEY (user_id),
    UNIQUE KEY uniq_user_account (account),
    UNIQUE KEY uniq_user_email (email),
    UNIQUE KEY uniq_user_phone (phone),
    KEY idx_user_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS user_info (
    user_info_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id      BIGINT UNSIGNED NOT NULL,
    username     VARCHAR(64)     NOT NULL,
    avatar_url   VARCHAR(512)    NULL,
    bio          VARCHAR(255)    NULL,
    follow_count INT UNSIGNED    NOT NULL DEFAULT 0,
    fans_count   INT UNSIGNED    NOT NULL DEFAULT 0,
    created_at   DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    updated_at   DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                                      ON UPDATE CURRENT_TIMESTAMP(3),
    PRIMARY KEY (user_info_id),
    UNIQUE KEY uniq_user_info_user (user_id),
    KEY idx_user_info_username (username),
    CONSTRAINT fk_user_info_user
        FOREIGN KEY (user_id) REFERENCES user (user_id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- One row per directed follow relationship. Fans are queried by followee_id.
-- status is denormalized: 0 = one-way follow, 1 = mutual follow.
CREATE TABLE IF NOT EXISTS user_follow (
    follower_id BIGINT UNSIGNED NOT NULL,
    followee_id BIGINT UNSIGNED NOT NULL,
    status      TINYINT         NOT NULL DEFAULT 0 COMMENT '0=单向关注,1=互关',
    created_at  DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    updated_at  DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                                     ON UPDATE CURRENT_TIMESTAMP(3),
    PRIMARY KEY (follower_id, followee_id),
    KEY idx_follow_followee_created (followee_id, created_at),
    KEY idx_follow_follower_created (follower_id, created_at),
    CONSTRAINT chk_follow_not_self CHECK (follower_id <> followee_id),
    CONSTRAINT fk_follow_follower
        FOREIGN KEY (follower_id) REFERENCES user (user_id)
        ON DELETE CASCADE,
    CONSTRAINT fk_follow_followee
        FOREIGN KEY (followee_id) REFERENCES user (user_id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- IM threads
-- ---------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS chat_thread (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    type        ENUM('private','group') NOT NULL,
    last_msg_id BIGINT UNSIGNED NOT NULL DEFAULT 0
                    COMMENT '冗余字段，用于会话列表排序',
    last_msg_at DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                    COMMENT '冗余字段，用于会话列表排序',
    created_at  DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (id),
    KEY idx_thread_last_msg_at (last_msg_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- The application must store user1_id < user2_id so a pair has one thread.
CREATE TABLE IF NOT EXISTS private_chat (
    thread_id  BIGINT UNSIGNED NOT NULL,
    user1_id   BIGINT UNSIGNED NOT NULL,
    user2_id   BIGINT UNSIGNED NOT NULL,
    created_at DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (thread_id),
    UNIQUE KEY uniq_private_pair (user1_id, user2_id),
    KEY idx_private_user1_thread (user1_id, thread_id),
    KEY idx_private_user2_thread (user2_id, thread_id),
    CONSTRAINT chk_private_pair_order CHECK (user1_id < user2_id),
    CONSTRAINT fk_private_thread
        FOREIGN KEY (thread_id) REFERENCES chat_thread (id)
        ON DELETE CASCADE,
    CONSTRAINT fk_private_user1
        FOREIGN KEY (user1_id) REFERENCES user (user_id),
    CONSTRAINT fk_private_user2
        FOREIGN KEY (user2_id) REFERENCES user (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS group_chat (
    thread_id  BIGINT UNSIGNED NOT NULL,
    name       VARCHAR(255)    NULL,
    owner_id   BIGINT UNSIGNED NOT NULL,
    created_at DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (thread_id),
    KEY idx_group_owner (owner_id),
    CONSTRAINT fk_group_thread
        FOREIGN KEY (thread_id) REFERENCES chat_thread (id)
        ON DELETE CASCADE,
    CONSTRAINT fk_group_owner
        FOREIGN KEY (owner_id) REFERENCES user (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS group_chat_member (
    thread_id   BIGINT UNSIGNED NOT NULL,
    user_id     BIGINT UNSIGNED NOT NULL,
    role        TINYINT         NOT NULL DEFAULT 0
                    COMMENT '0=普通成员,1=管理员,2=创建者',
    joined_at   DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    muted_until DATETIME(3)     NULL,
    PRIMARY KEY (thread_id, user_id),
    KEY idx_group_member_user (user_id, thread_id),
    CONSTRAINT fk_group_member_thread
        FOREIGN KEY (thread_id) REFERENCES chat_thread (id)
        ON DELETE CASCADE,
    CONSTRAINT fk_group_member_user
        FOREIGN KEY (user_id) REFERENCES user (user_id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- Messages and read receipts
-- ---------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS chat_message (
    message_id     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    thread_id      BIGINT UNSIGNED NOT NULL,
    thread_msg_seq BIGINT UNSIGNED NOT NULL
                       COMMENT '会话内单调递增序号，排序/同步/去重',
    sender_id      BIGINT UNSIGNED NOT NULL,
    client_msg_id  VARCHAR(64)     NULL
                       COMMENT '客户端消息ID，用于重试幂等',
    msg_type       TINYINT         NOT NULL
                       COMMENT '1=文本,2=图片,3=语音,4=视频,5=文件,6=表情,7=系统,8=语音通话,9=视频通话,10=自定义',
    content        TEXT            NOT NULL
                       COMMENT '文本内容或JSON结构化载荷',
    status         TINYINT         NOT NULL DEFAULT 0
                       COMMENT '0=正常,1=已撤回',
    created_at     DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (message_id),
    UNIQUE KEY uniq_thread_seq (thread_id, thread_msg_seq),
    UNIQUE KEY uniq_sender_client_msg (thread_id, sender_id, client_msg_id),
    KEY idx_message_thread_created (thread_id, created_at),
    KEY idx_message_thread_id (thread_id, message_id),
    KEY idx_message_sender_created (sender_id, created_at),
    CONSTRAINT fk_message_thread
        FOREIGN KEY (thread_id) REFERENCES chat_thread (id)
        ON DELETE CASCADE,
    CONSTRAINT fk_message_sender
        FOREIGN KEY (sender_id) REFERENCES user (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- One row per user and thread; avoids storing per-user read state on every message.
CREATE TABLE IF NOT EXISTS chat_message_read (
    thread_id     BIGINT UNSIGNED NOT NULL,
    user_id       BIGINT UNSIGNED NOT NULL,
    last_read_seq BIGINT UNSIGNED NOT NULL DEFAULT 0,
    updated_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                                      ON UPDATE CURRENT_TIMESTAMP(3),
    PRIMARY KEY (thread_id, user_id),
    KEY idx_read_user (user_id, updated_at),
    CONSTRAINT fk_read_thread
        FOREIGN KEY (thread_id) REFERENCES chat_thread (id)
        ON DELETE CASCADE,
    CONSTRAINT fk_read_user
        FOREIGN KEY (user_id) REFERENCES user (user_id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- Video / feed
-- ---------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS video (
    id              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    author_id       BIGINT UNSIGNED NOT NULL,
    request_id      VARCHAR(64)     NULL
                        COMMENT 'client upload idempotency key',
    title           VARCHAR(200)    NOT NULL,
    description     TEXT            NULL,
    category_id     INT UNSIGNED    NOT NULL,
    cover_key       VARCHAR(512)    NOT NULL,
    duration_ms     INT UNSIGNED    NOT NULL DEFAULT 0,
    width           SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    height          SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    status          TINYINT UNSIGNED NOT NULL DEFAULT 0
                        COMMENT '0=草稿,1=上传中,2=转码中,3=可用,4=处理失败,5=已下架',
    review_status   TINYINT UNSIGNED NOT NULL DEFAULT 0
                        COMMENT '0=待审核,1=审核通过,2=审核拒绝,3=人工复审',
    visibility      TINYINT UNSIGNED NOT NULL DEFAULT 0
                        COMMENT '0=公开,1=私密,2=仅关注者可见,3=不公开但可通过链接访问',
    published_at    DATETIME(3)     NULL,
    created_at      DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    updated_at      DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                                      ON UPDATE CURRENT_TIMESTAMP(3),
    deleted_at      DATETIME(3)     NULL,
    PRIMARY KEY (id),
    KEY idx_feed (status, review_status, visibility, published_at, id),
    KEY idx_category (category_id, status, published_at, id),
    KEY idx_author (author_id, published_at, id),
    UNIQUE KEY uniq_author_request (author_id, request_id),
    CONSTRAINT fk_video_author
        FOREIGN KEY (author_id) REFERENCES user (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS video_stat (
    video_id        BIGINT UNSIGNED NOT NULL,
    play_count      BIGINT UNSIGNED NOT NULL DEFAULT 0,
    like_count      BIGINT UNSIGNED NOT NULL DEFAULT 0,
    comment_count   BIGINT UNSIGNED NOT NULL DEFAULT 0,
    share_count     BIGINT UNSIGNED NOT NULL DEFAULT 0,
    favorite_count  BIGINT UNSIGNED NOT NULL DEFAULT 0,
    report_count    BIGINT UNSIGNED NOT NULL DEFAULT 0
                        COMMENT '举报次数，用于推荐降权',
    completion_rate DECIMAL(5,4)    NOT NULL DEFAULT 0.0000,
    avg_watch_ms    INT UNSIGNED    NOT NULL DEFAULT 0,
    updated_at      DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                                      ON UPDATE CURRENT_TIMESTAMP(3),
    PRIMARY KEY (video_id),
    CONSTRAINT fk_video_stat_video
        FOREIGN KEY (video_id) REFERENCES video (id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS video_asset (
    id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    video_id    BIGINT UNSIGNED NOT NULL,
    asset_type  ENUM('original','hls','cover','preview') NOT NULL,
    quality     VARCHAR(32)     NOT NULL DEFAULT 'default'
                    COMMENT 'default=封面/原片, master=HLS主列表, 360p/720p/1080p=清晰度',
    width       SMALLINT UNSIGNED NULL,
    height      SMALLINT UNSIGNED NULL,
    bitrate     INT UNSIGNED    NULL,
    codec       VARCHAR(32)     NULL,
    storage_key VARCHAR(512)    NOT NULL,
    file_size   BIGINT UNSIGNED NULL,
    created_at  DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (id),
    UNIQUE KEY uniq_asset (video_id, asset_type, quality),
    KEY idx_asset_video (video_id),
    CONSTRAINT fk_video_asset_video
        FOREIGN KEY (video_id) REFERENCES video (id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS tag (
    id   INT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '标签主键，自增',
    name VARCHAR(64)  NOT NULL                COMMENT '标签名称，全局唯一，直接展示给用户',
    PRIMARY KEY (id),
    UNIQUE KEY uniq_tag_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='标签字典：视频分类与检索用的固定词条，只存名称，不含业务统计';

CREATE TABLE IF NOT EXISTS video_tag (
    video_id BIGINT UNSIGNED NOT NULL,
    tag_id   INT UNSIGNED    NOT NULL,
    PRIMARY KEY (video_id, tag_id),
    KEY idx_tag_video (tag_id, video_id),
    CONSTRAINT fk_video_tag_video
        FOREIGN KEY (video_id) REFERENCES video (id)
        ON DELETE CASCADE,
    CONSTRAINT fk_video_tag_tag
        FOREIGN KEY (tag_id) REFERENCES tag (id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS editorial_pick (
    id         BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    video_id   BIGINT UNSIGNED NOT NULL,
    channel    VARCHAR(32)     NOT NULL DEFAULT 'curated',
    slot_type  VARCHAR(32)     NOT NULL DEFAULT 'auto',
    position   INT             NOT NULL DEFAULT 0
                   COMMENT '同频道同位置的固定排序，数值越小越靠前',
    weight     DECIMAL(10,4)   NOT NULL DEFAULT 0.0000,
    starts_at  DATETIME(3)     NULL,
    ends_at    DATETIME(3)     NULL,
    enabled    TINYINT UNSIGNED NOT NULL DEFAULT 1,
    created_at DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (id),
    KEY idx_pick_active (channel, enabled, starts_at, ends_at),
    CONSTRAINT fk_editorial_video
        FOREIGN KEY (video_id) REFERENCES video (id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS video_like (
    user_id    BIGINT UNSIGNED NOT NULL,
    video_id   BIGINT UNSIGNED NOT NULL,
    created_at DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (user_id, video_id),
    KEY idx_video_like_video (video_id, created_at),
    CONSTRAINT fk_video_like_user
        FOREIGN KEY (user_id) REFERENCES user (user_id)
        ON DELETE CASCADE,
    CONSTRAINT fk_video_like_video
        FOREIGN KEY (video_id) REFERENCES video (id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS video_comment (
    id         BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    video_id   BIGINT UNSIGNED NOT NULL,
    user_id    BIGINT UNSIGNED NOT NULL,
    content    TEXT            NOT NULL,
    status     TINYINT UNSIGNED NOT NULL DEFAULT 0
                   COMMENT '0=正常,1=已隐藏,2=已删除',
    created_at DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    updated_at DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                                  ON UPDATE CURRENT_TIMESTAMP(3),
    PRIMARY KEY (id),
    KEY idx_comment_video (video_id, created_at),
    KEY idx_comment_user (user_id, created_at),
    CONSTRAINT fk_video_comment_video
        FOREIGN KEY (video_id) REFERENCES video (id)
        ON DELETE CASCADE,
    CONSTRAINT fk_video_comment_user
        FOREIGN KEY (user_id) REFERENCES user (user_id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
