-- ============================================================
-- tag 表：视频标签字典
-- 说明：
--   1. tag 只存标签词条本身，点赞/播放等统计不放在这里；
--   2. 视频和标签是多对多，关联表是 video_tag(video_id, tag_id)；
--   3. 标签是给用户看的展示词，不是检索索引，检索靠 video_tag 的 idx_tag_video。
-- ============================================================

-- 给表和字段补注释（MySQL 的 COMMENT 存在表结构里，DESC / SHOW CREATE TABLE 都能看到）
ALTER TABLE tag
    COMMENT = '标签字典：视频分类与检索用的固定词条，只存名称，不含业务统计',
    MODIFY COLUMN id   INT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '标签主键，自增',
    MODIFY COLUMN name VARCHAR(64)  NOT NULL                COMMENT '标签名称，全局唯一，直接展示给用户';

-- 初始标签：INSERT IGNORE + unique(name)，重复执行不会报错也不会产生重复行
INSERT IGNORE INTO tag (name) VALUES
    ('影视'),   -- 电影 / 电视剧 / 动画的剪辑与解说
    ('音乐'),   -- 歌曲、翻唱、乐器演奏、现场
    ('游戏'),   -- 实况、攻略、赛事、独立游戏
    ('美食'),   -- 探店、做饭教程、吃播
    ('旅行'),   -- 风景、旅行攻略、城市 vlog
    ('搞笑'),   -- 段子、整活、鬼畜二创
    ('体育'),   -- 赛事集锦、球类、田径等竞技内容
    ('科技'),   -- 编程、AI、硬件原理、行业观察
    ('数码'),   -- 手机 / 电脑 / 外设的开箱与测评
    ('汽车'),   -- 新车、改装、试驾、用车知识
    ('时尚'),   -- 穿搭、潮流、配饰
    ('萌宠')   -- 猫狗等宠物的日常与科普
;

SELECT id, name FROM tag ORDER BY id;
