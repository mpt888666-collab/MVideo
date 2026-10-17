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
