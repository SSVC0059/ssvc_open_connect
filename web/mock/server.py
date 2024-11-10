from flask import Flask, Response, request, jsonify
from flask_cors import CORS
import json
import os
import sys
import time

app = Flask(__name__)
CORS(app)

current_command = ""

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
        global current_command
        _db_file = ""
        while True:
            if current_command != "":
                print(current_command)
                dir = os.path.dirname(db_file)
                responce_file = os.path.join(dir, f"{current_command}.json")
                _db_file = db_file
                db = responce_file
            else:
                db = db_file
            with open(db) as f:
                data = json.load(f)
                yield f"data: {json.dumps(data)}\n\n"
            if current_command != "":
                current_command = ""
            time.sleep(1)  # Отправка данных каждую секунду

    return Response(generate(), mimetype='text/event-stream')

@app.route('/data')
def data():
    with open(db_file) as f:
        data = json.load(f)
    return data


@app.route('/command', methods=['POST'])
def execute_command():
    # Получаем значение команды из JSON
    global current_command
    data = request.json
    current_command = data.get('command')

    # Возвращаем результат выполнения команды
    return jsonify({
        "command": current_command,
        "response": "Ок"
    })

if __name__ == '__main__':
    app.run(port=3000)
