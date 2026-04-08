INSERT INTO users (name, age, major) VALUES ('김민준', 25, '컴퓨터공학');
INSERT INTO users (name, age, major) VALUES ('이서연', 22, '경영학');
INSERT INTO users (name, age, major) VALUES ('박지호', 23, '물리학');
INSERT INTO users (name, age, major) VALUES ('최유나', 21, '화학');
INSERT INTO users (major, name, age) VALUES ('수학', '정현우', 24);

SELECT * FROM users ORDER BY age ASC;
SELECT name, age FROM users WHERE age = 22;
SELECT name, age FROM users WHERE age != 22 ORDER BY age ASC LIMIT 3;
SELECT name, age FROM users WHERE age > 22 ORDER BY age DESC LIMIT 2;
SELECT name, age FROM users WHERE age < 23 ORDER BY name ASC;
SELECT name, age FROM users WHERE age >= 24 ORDER BY age DESC;
SELECT name, age FROM users WHERE age <= 22 ORDER BY age ASC;

UPDATE users SET major = '전자공학' WHERE name = '김민준';
UPDATE users SET age = 30, major = '데이터사이언스' WHERE name = '정현우';
SELECT * FROM users ORDER BY age DESC;

DELETE FROM users WHERE age < 23;
SELECT * FROM users ORDER BY age ASC;

DELETE FROM users;
SELECT * FROM users;
