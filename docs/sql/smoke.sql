-- Minimal write/read smoke test for docs/sql/schema.sql
USE MVideo;

INSERT INTO user (account, phone, email, password_hash)
VALUES ('ci_user1', NULL, 'ci1@example.com', 'hash');
SET @u1 = LAST_INSERT_ID();

INSERT INTO user (account, phone, email, password_hash)
VALUES ('ci_user2', NULL, 'ci2@example.com', 'hash');
SET @u2 = LAST_INSERT_ID();

INSERT INTO user_info (user_id, username)
VALUES (@u1, 'ci1'), (@u2, 'ci2');

INSERT INTO user_follow (follower_id, followee_id, status)
VALUES (@u1, @u2, 0);

INSERT INTO chat_thread (type) VALUES ('private');
SET @t = LAST_INSERT_ID();

INSERT INTO private_chat (thread_id, user1_id, user2_id)
VALUES (@t, @u1, @u2);

INSERT INTO chat_message
    (thread_id, thread_msg_seq, sender_id, client_msg_id, msg_type, content)
VALUES
    (@t, 1, @u1, 'ci-msg-1', 1, 'hello');

INSERT INTO chat_message_read (thread_id, user_id, last_read_seq)
VALUES (@t, @u2, 1);

SELECT 'smoke_ok' AS result, COUNT(*) AS messages FROM chat_message;
