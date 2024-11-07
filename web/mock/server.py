from flask import Flask, Response
from flask_cors import CORS
import json
import os
import sys
import time

app = Flask(__name__)
CORS(app)

# Параметры командной строки
if len(sys.argv) < 2:
    print("Usage: python server.py <database.json>")
    sys.exit(1)

db_file = sys.argv[1]

# Проверка существования файла
if not os.path.exists(db_file):
    print(f"File {db_file} does not exist.")
    sys.exit(1)

@app.route('/events')
def events():
    def generate():
        while True:
            with open(db_file) as f:
                data = json.load(f)
                yield f"data: {json.dumps(data)}\n\n"
            time.sleep(1)  # Отправка данных каждую секунду

    return Response(generate(), mimetype='text/event-stream')

@app.route('/data')
def data():
    with open(db_file) as f:
        data = json.load(f)
    return data

if __name__ == '__main__':
    app.run(port=3000)
