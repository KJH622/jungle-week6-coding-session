INSERT INTO users (name, age, major) VALUES ('김민준', 25, '컴퓨터공학');
INSERT INTO users (name, age, major) VALUES ('이서연', 22, '경영학');
INSERT INTO users (name, age, major) VALUES ('박지호', 23, '물리학');
INSERT INTO users (name, age, major) VALUES ('최유나', 21, '화학');

SELECT name FROM users WHERE age > 22 ORDER BY age DESC LIMIT 2;

UPDATE users SET major = '전자공학' WHERE name = '김민준';
SELECT name, major FROM users WHERE name = '김민준';

DELETE FROM users WHERE age < 22;
SELECT * FROM users ORDER BY age ASC;

INSERT INTO users (major, name, age) VALUES ('수학', '정현우', 24);
SELECT * FROM users ORDER BY age DESC LIMIT 2;
