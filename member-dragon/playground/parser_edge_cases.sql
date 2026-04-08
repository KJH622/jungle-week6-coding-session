INSERT INTO users (name, age, major) VALUES ('O''Reilly', 30, 'Books');
INSERT INTO users (name, age, major) VALUES ('정현우', 24, '수학');
INSERT INTO products (name, price, category) VALUES ('SELECT FROM WHERE LIMIT', 1000, 'keyword test');
INSERT INTO products (name, price, category) VALUES ('세미;콜론', 1200, 'cat;egory');

SELECT * FROM users WHERE name = 'O''Reilly';
UPDATE users SET age = 31 WHERE name = 'O''Reilly';
SELECT name, age FROM users WHERE age > 20 ORDER BY age DESC LIMIT 2;
SELECT * FROM products ORDER BY price DESC LIMIT 2
