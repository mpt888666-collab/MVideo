ALTER TABLE video
    ADD COLUMN request_id VARCHAR(64) NULL
        COMMENT 'client upload idempotency key' AFTER author_id,
    ADD UNIQUE KEY uniq_author_request (author_id, request_id);
