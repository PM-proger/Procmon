#
# Простой сервер для взаимодействия с программой PROCMON:
#   - получает данные от PROCMON (зашифрованный перечень DLL),
#   - возвращает в PROCMON ранее полученные данные.
# Параметры запуска: отсутствуют.
# Адрес и порт прослушивания: 127.0.0.1:80.
#
from http.server import HTTPServer, BaseHTTPRequestHandler
import json
import os

# Глобальные константы
DB_FILE_NAME = 'procmon_db.json'        # здесь сохраняем данные клиента
HTTP_ADDR = "127.0.0.1"                 # адрес сервера (сшуаем его)
HTTP_PORT = 80                          # ... и порт сервера ...



def read_user_data_from_file(file_name):
    # читает ранее сохраненные данные из файла _file_name_.
    # Возвращает прочитанные данные в виде словаря
    # Каждая запись файла КЛЮЧ:ЗНАЧЕНИЕ соответствует паре rid:data
    try:
        f = open(file_name, 'r', encoding='utf-8')
        data = json.load(f)
        f.close()

        print(f"Файл: {file_name} -- прочитан успешно.")
        return data

    except Exception as e:
        print(f"Ошибка чтения файла {file_name}  --> ", e)
        return []

def save_user_data(db, user_data):
    # Сохраняет полученные данные _user_data_ {cmd - игнорируется, rid, data}
    # в базе данных _db_ для последующей выдачи пользователю по запросу с cmd==2.
    # user_data["rid"] - ключ, user_data["data"] - значение
    db[user_data["rid"]] = user_data["data"]

    save_right_now = True
    if (save_right_now):
        # сразу сохраняем данные в файле
        db_file = open(DB_FILE_NAME, 'w', encoding='utf-8')
        json.dump(db, db_file)
        file_name = db_file.name
        full_name = os.path.abspath(file_name)
        print(f"Данные сохранены в файле: {full_name}")
        db_file.close()

    return

def get_user_data(db, key):
    # Возвращает данные по ключу _key_ из базы данных _db_
    # или None, если такая запись отсутствует.
    if key in db:
        r = db[key]
    else:
        r = None
    return r


def send_response_to_client(in_self, in_rid, in_data, in_status):
    # Направляет ответ клиенту.
    # in_self : BaseHTTPRequestHandler
    # in_rid и in_status направляются всегда.
    # in_data направляется только если != None

    # формируем текст ответа
    response_json = {"rid": in_rid, "status": "true" if in_status else "false"}

    if in_data != None:
        response_json["data"] = in_data

    response_str = json.dumps(response_json).encode('utf-8')

    # подтверждение http (код = 200)
    in_self.send_response(200)
    in_self.send_header("Content-Type", "text/plain; charset=utf-8")
    in_self.end_headers()

    #формируем ответ сервера
    in_self.wfile.write(response_str)   

    return

class PostHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        content_length = int(self.headers['Content-Length'])
        data = self.rfile.read(content_length)
        
        # Теперь можно обработать данные (например, распарсить JSON или форму)
        print(f"Получен POST-запрос: {data}")

        try:
            parsed_data = json.loads(data)
        except json.JSONDecodeError as e:
            print(f"Ошибка парсинга JSON: {e}")

        
        # проверим ответ на наличие обязательных полей (остальные игнорируются)
        required_fields = ["cmd", "rid"]
        req_field_error = False
        for f in required_fields:
            if f not in parsed_data:
                print(f"ОШИБКА: В запросе отсутствует обязательное поле {f}")
                req_field_error = True

        if (req_field_error):
            return
        
        rid = parsed_data["rid"]
        cmd_code = parsed_data["cmd"]

        match cmd_code:
            case 1:
                print("---обработка команды = 1 (ЗАПИСЬ ДАННЫХ КЛИЕНТА НА СЕРВЕР)")

                # для команды записи сть еще обяз. поле - 'data'
                cmd_error = "data" not in parsed_data
                if cmd_error:
                    print("ОШИБКА: В запросе отсутствует обязательное поле 'data")
                    send_response_to_client(self, rid, None, False)
                else:
                    send_response_to_client(rid, None, True)
                    save_user_data(self, data_base, parsed_data)

            case 2:
                print("---обработка команды = 2 (чтение данных)")
                #ответ сервера -- {“rid”: “%УНИКАЛЬНЫЙ_ИДЕНТИФИКАТОР%”, “data”: “%ШИФРОВАННАЯ_СТРОКА%”}

                data_for_client = get_user_data(data_base, rid)
                status = data_for_client != None
                send_response_to_client(self, rid, data_for_client, status)


            case _:
                print("---  НЕКОРРЕКТНЫЙ КОД КОМАНДЫ = ${cmd_code}}")
        
        # Отправляем ответ клиенту
        return
    

server_address = (HTTP_ADDR, HTTP_PORT)

httpd = HTTPServer(server_address, PostHandler)

os.system("cls")

# читаем ранее сохраненные данные
data_base = read_user_data_from_file(DB_FILE_NAME)

try:
    print(f"Ожидаем запрос HTTP. Адрес: {HTTP_ADDR}   Порт: {HTTP_PORT}  CTRL-C для завершения...")
    httpd.serve_forever()
except KeyboardInterrupt:
    print("Сервер завершил работу.")
    httpd.server_close()