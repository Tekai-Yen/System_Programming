import socket
import time
import sys
import datetime
import random
import string
import traceback
from tqdm import tqdm
from collections import Counter
from enum import IntEnum, auto
from pwnlib.tubes.remote import remote
from pwnlib.tubes.process import process

RACE_TOLERANCE = 500
ACTUAL_TOLERANCE = None
MAX_CONN_TIME = 5000
TIMEOUT = .5
DEVNULL = open('/dev/null', 'w+b')


def red(str_, io=sys.stdout):
    print("\33[31m" + str_ + "\33[0m", file=io)


def green(str_, io=sys.stdout):
    print("\33[32m" + str_ + "\33[0m", file=io)


def yellow(str_, io=sys.stdout):
    print("\33[33m" + str_ + "\33[0m", file=io)


class ResponseChecker():
    def __init__(self):
        self.BANNER = 0
        self.LOCK = 1
        self.EXIT = 2
        self.CANCEL = 3
        self.FULL = 4
        self.BOOKED = 5
        self.NOPAID = 6
        self.SUCCESS = 7
        self.INVALID = 8
        self.CHECK = 9
        self.BOOK = 10
        self.SEAT = 11
        self.CONT = 12
        self.response = [
            [
                b"======================================\n",
                b" Welcome to CSIE Train Booking System \n",
                b"======================================\n"
            ],
            b">>> Locked.\n",
            b">>> Client exit.\n",
            b">>> You cancel the seat.\n",
            b">>> The shift is fully booked.\n",
            b">>> The seat is booked.\n",
            b">>> No seat to pay.\n",
            b">>> Your train booking is successful.\n",
            b">>> Invalid operation.\n",
            b"Please select the shift you want to check [902001-902005]: ",
            b"Please select the shift you want to book [902001-902005]: ",
            b"Select the seat [1-40] or type \"pay\" to confirm: ",
            b"Type \"seat\" to continue or \"exit\" to quit [seat/exit]: ",
        ]


rc = ResponseChecker()


def find_empty_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('localhost', 0))
    _, port = s.getsockname()
    s.close()
    return port


class WrongAnswer(Exception):
    def __init__(self, message="Wrong Answer"):
        self.message = message
        super().__init__(self.message)

    def __str__(self):
        return self.message


class SeatState(IntEnum):
    NOTHING = 0
    RESERVED = 2
    BOOKED = 1


train_info = {}


def generate_train_info():
    train_list = [902001, 902002, 902003, 902004, 902005]
    full = random.choice(train_list)
    for num in train_list:
        if num != full:
            train_info[num] = [(SeatState.NOTHING if random.random() < 0.8 else SeatState.BOOKED)
                               for i in range(40)]
        else:
            train_info[num] = [SeatState.BOOKED] * 40
        with open(f"csie_trains/train_{num}", "w") as f:
            for i in range(40):
                f.write(str(int(train_info[num][i])))
                f.write('\n' if i % 4 == 3 else ' ')


class Server:
    def __init__(self):
        self.log_name = self.generate_random_suffix()
        self.port = find_empty_port()
        self.server: process

    def kill(self):
        self.server.kill()

    @classmethod  # thanks, ChatGPT!
    def generate_random_suffix(cls, length=8):
        """Generate a random string of fixed length."""
        letters = string.ascii_lowercase + string.digits  # You can adjust this as needed
        return ''.join(random.choice(letters) for _ in range(length))


class WriteServer(Server):
    def __init__(self):
        super().__init__()
        self.log_file = open(f"./logs/server-{self.log_name}.log", "w+")
        print(f"> write server log @server-{self.log_name}.log {self.port}")
        self.server = process(
            ['./write_server', str(self.port)], stderr=self.log_file, stdout=self.log_file)


class ReadServer(Server):
    def __init__(self):
        super().__init__()
        # self.log_file = open('/dev/null', 'w+b')
        self.log_file = open(f"./logs/server-{self.log_name}.log", "w+")
        print(f"> read server log @server-{self.log_name}.log {self.port}")
        self.server = process(
            ['./read_server', str(self.port)], stderr=self.log_file, stdout=self.log_file)


class Client(remote):
    def __init__(self, port, busted):
        super().__init__('localhost', port)
        self.timestamp = datetime.datetime.now()
        self.busted = busted
        self.invalid = False
        self.paused = False
        self.remain = b''
        self.check_line(rc.response[rc.BANNER][0])
        self.check_line(rc.response[rc.BANNER][1])
        self.check_line(rc.response[rc.BANNER][2])

    def check_line(self, expected):
        res = self.recvline(timeout=TIMEOUT)
        if res != expected:
            raise WrongAnswer(f"Found: {res}, Expected: {expected}")

    def check_prompt(self, expected):
        res = self.recv(timeout=TIMEOUT)
        if res != expected:
            raise WrongAnswer(f"Found: {res}, Expected: {expected}")

    @property
    def remaining_time(self):
        return MAX_CONN_TIME - (datetime.datetime.now() - self.timestamp).total_seconds() * 1000

    def check_closed(self):
        if self.invalid:
            return True
        try:
            # this should read all content in the buffer
            receive = self.recv(timeout=.1)
            self.recv(timeout=.1)           # this should read nothing
        except EOFError:
            return True
        if len(receive) > 0:
            self.unrecv(receive)
        return False

    def check_timeout(self):
        if self.remaining_time < -100:
            if not self.check_closed():
                print(self.remaining_time)
                raise WrongAnswer("Connection closed too late.")
        if self.check_closed():
            if self.remaining_time > 100:
                print(self.remaining_time)
                raise WrongAnswer("Connection closed too early.")
            self.invalid = True
            self.close()
            return True

        return False

    def custom_send(self, msg):
        if not self.busted:
            self.sendline(msg)
            return True
        else:
            if self.remain == b'':
                self.remain = msg + b'\n'
            l = len(self.remain)
            p = max(random.randint(1, l), random.randint(1, l))
            out, self.remain = self.remain[0:p], self.remain[p:]
            self.send(out)
            if self.remain == b'':
                return True
            self.paused = True
            return False


class ReadClient(Client):
    def __init__(self, port, busted=False):
        super().__init__(port, busted)
        self.read_prompt()
        # ========================
        self.rng_do_exit: bool
        self.rng_shift: int
        # ========================

    def read_prompt(self):
        try:
            # receive prompt
            self.check_prompt(rc.response[rc.CHECK])
        except EOFError:
            # acceptable, check if time range is resonable
            self.check_timeout()

    def decision(self):
        self.rng_do_exit = (random.random() < 0.1)
        self.rng_shift = random.choice(range(902001, 902006))

    def random_walk(self):
        if self.invalid:
            return

        # print(self.remaining_time)

        if self.check_timeout():
            return

        try:

            if not self.paused:
                self.decision()

            if self.rng_do_exit:
                if not self.custom_send(b'exit'):
                    return
                self.check_line(rc.response[rc.EXIT])
                self.invalid = True
                self.close()
                return

            # query some of the train
            else:
                shift = self.rng_shift
                while race_resolver():
                    time.sleep(.2)
                if not self.custom_send(str(shift).encode()):
                    return
                for i in range(10):
                    self.check_line(
                        (' '.join([str(int(train_info[shift][i * 4 + j])) for j in range(4)]) + '\n').encode())

            # receive prompt, we should do it this way so the server can have some rest
            self.check_prompt(rc.response[rc.CHECK])
        except EOFError:
            # acceptable, check if time range is resonable
            self.check_timeout()


class WriteState(IntEnum):
    CHOOSE_SHIFT = auto()
    SELECT_SEAT = auto()
    CONTINUE_OR_EXIT = auto()
    FINISHED = auto()


# client that actually has lock. check if (at most) 200 client is alive
important_client = Counter()


class WriteClient(Client):
    def __init__(self, port, busted=False):
        super().__init__(port, busted)
        self.status = WriteState.CHOOSE_SHIFT
        self.shift = 0
        self.book_map = [SeatState.NOTHING] * 40
        self.pending = 0
        # ========================
        self.rng_do_exit: bool
        self.rng_shift: int
        self.rng_do_pay: bool
        self.rng_index: int
        # ========================
        self.read_prompt()

    def decision(self, exit_factor, pay_factor):
        self.rng_do_exit = (random.random() < exit_factor)
        self.rng_shift = random.choice(range(902001, 902006))
        self.rng_do_pay = (random.random() < pay_factor)
        self.rng_index = random.randint(0, 39)

    def read_prompt(self):
        try:
            # receive prompt
            if self.status == WriteState.CHOOSE_SHIFT:
                self.check_prompt(rc.response[rc.BOOK])
            elif self.status == WriteState.SELECT_SEAT:
                self.check_prompt(rc.response[rc.SEAT])
            elif self.status == WriteState.CONTINUE_OR_EXIT:
                self.check_prompt(rc.response[rc.CONT])
        except EOFError:
            # acceptable, check if time range is resonable
            self.check_timeout()

    def cleanup(self):
        if self.invalid:
            for i in range(40):
                if self.book_map[i] == SeatState.RESERVED:
                    self.book_map[i] = train_info[self.shift][i] = SeatState.NOTHING
            if self.pending > 0:
                self.pending = 0
                del important_client[self]

    def random_walk(self, exit_factor=0.02, pay_factor=0.05):
        if self.invalid:
            return

        if self.check_timeout():
            return

        try:
            # exit prematurely
            if not self.paused:
                self.decision(exit_factor, pay_factor)

            if self.rng_do_exit:
                if not self.custom_send(b'exit'):
                    return
                self.check_line(rc.response[rc.EXIT])
                self.invalid = True
                self.close()
                return

            # three statuses:
            if self.status == WriteState.CHOOSE_SHIFT:
                # select some shift
                self.shift = self.rng_shift
                if not self.custom_send(str(self.shift).encode()):
                    return

                if train_info[self.shift] == [SeatState.BOOKED] * 40:
                    self.check_line(rc.response[rc.FULL])
                else:
                    self.status = WriteState.SELECT_SEAT

            elif self.status == WriteState.SELECT_SEAT:

                # pay for the seats
                if self.rng_do_pay < pay_factor:
                    if not self.custom_send(b'pay'):
                        return

                    if self.pending == 0:
                        self.check_line(rc.response[rc.NOPAID])
                    if self.pending > 0:
                        self.check_line(rc.response[rc.SUCCESS])
                        for i in range(40):
                            if self.book_map[i] == SeatState.RESERVED:
                                self.book_map[i] = train_info[self.shift][i] = SeatState.BOOKED
                        self.pending = 0
                        self.status = WriteState.CONTINUE_OR_EXIT
                        del important_client[self]
                else:
                    index = self.rng_index
                    while race_resolver():
                        time.sleep(.2)
                    if not self.custom_send(str(index + 1).encode()):
                        return

                    if self.book_map[index] == SeatState.NOTHING:
                        if train_info[self.shift][index] == SeatState.NOTHING:
                            self.book_map[index] = train_info[self.shift][index] = SeatState.RESERVED
                            important_client[self] += 1
                            self.pending += 1
                        elif train_info[self.shift][index] == SeatState.RESERVED:
                            self.check_line(rc.response[rc.LOCK])
                        elif train_info[self.shift][index] == SeatState.BOOKED:
                            self.check_line(rc.response[rc.BOOKED])
                    elif self.book_map[index] == SeatState.RESERVED:
                        self.check_line(rc.response[rc.CANCEL])
                        self.book_map[index] = train_info[self.shift][index] = SeatState.NOTHING
                        important_client.subtract([self])
                        self.pending -= 1
                    elif self.book_map[index] == SeatState.BOOKED:
                        self.check_line(rc.response[rc.BOOKED])

            elif self.status == WriteState.CONTINUE_OR_EXIT:
                if not self.custom_send(b'seat'):
                    return
                self.status = WriteState.SELECT_SEAT

            # check next input
            if self.status != WriteState.CHOOSE_SHIFT:
                self.check_line(b'\n')
                self.check_line(b'Booking info\n')
                self.check_line(f'|- Shift ID: {self.shift}\n'.encode())
                self.check_line(
                    f'|- Chose seat(s): {",".join([str(i + 1) for i in range(40) if self.book_map[i] == SeatState.RESERVED])}\n'.encode())
                self.check_line(
                    f'|- Paid: {",".join([str(i + 1) for i in range(40) if self.book_map[i] == SeatState.BOOKED])}\n'.encode())
                self.check_line(b'\n')

            self.read_prompt()
        except EOFError:
            # acceptable, check if time range is resonable
            self.check_timeout()

# dirty hack for the lock race condition, we need to resolve exactly before we read any input
# if any resolution still causes change, we shall resolve again until there is no changes.


def race_resolver():
    global ACTUAL_TOLERANCE
    potential_race = False
    for j in list(important_client.keys()):
        j: WriteClient
        if j.check_timeout():
            j.cleanup()
            if j.remaining_time < ACTUAL_TOLERANCE:
                # print(ACTUAL_TOLERANCE)
                potential_race = True
        # print("resolving ", len(important_client))
    return potential_race


def run(testcase, name: str):
    global important_client

    important_client = Counter()
    print(f"Subtask {name}:")
    try:
        testcase()
    except Exception as e:
        red("Wrong Answer", io=sys.stdout)
        print("\tReason: ", e)
        print(traceback.format_exc())
        return
    green("Success!", io=sys.stdout)

# ---------------------------------------------------------------------------------------------


def testcase4read():
    N = 1000
    Q = 10000
    print(f"(read client {N}, query {Q}, has reconnection)")

    try:
        generate_train_info()
        server = ReadServer()
        time.sleep(2)
        client: list[ReadClient] = []
        for _ in range(N):
            client.append(ReadClient(server.port))

        for _ in tqdm(range(Q)):
            i = random.randint(0, N - 1)
            client[i].random_walk()
            if client[i].invalid:
                client[i] = ReadClient(server.port)
    finally:
        server.kill()
        for conn in client:
            conn.close()


def testcase4write1():
    N = 200
    Q = 2000
    print(f"(write client {N}, query {Q}, has reconnection)")
    # print("(write client 200, query 2000, has reconnection)")

    try:
        generate_train_info()
        server = WriteServer()
        time.sleep(1)
        client: list[WriteClient] = []
        for _ in range(N):
            client.append(WriteClient(server.port))

        for _ in tqdm(range(Q)):
            i = random.randint(0, N - 1)
            client[i].random_walk()
            client[i].cleanup()
            if client[i].invalid:
                client[i] = WriteClient(server.port)
    finally:
        server.kill()
        for conn in client:
            conn.close()


def testcase4write2():
    N = 20
    Q = 1000
    print(f"(write client {N}, query {Q}, has reconnection)")
    # print("(write client 20, query 1000, pay more, has reconnection)")

    try:
        generate_train_info()
        server = WriteServer()
        time.sleep(1)
        client: list[WriteClient] = []
        for _ in range(N):
            client.append(WriteClient(server.port))

        for _ in tqdm(range(Q)):
            i = random.randint(0, N - 1)
            # print(i + 9)
            client[i].random_walk(exit_factor=0.01, pay_factor=0.2)
            client[i].cleanup()
            if client[i].invalid:
                client[i] = WriteClient(server.port)
    finally:
        server.kill()
        for conn in client:
            conn.close()


def testcase5stress():
    N = 200
    M = 200
    Q = 1000
    print("(read/write client 200/200, read/write server 2/2, query 1000 (read:write 3:7), has reconnection)")

    try:
        generate_train_info()
        write_servers = [WriteServer(), WriteServer()]
        read_servers = [ReadServer(), ReadServer()]
        time.sleep(1)
        write_clients: list[WriteClient] = []
        read_clients: list[ReadClient] = []

        def new_write_client():
            return WriteClient(write_servers[random.randint(0, 1)].port)

        def new_read_client():
            return ReadClient(read_servers[random.randint(0, 1)].port)

        for _ in range(N):
            write_clients.append(new_write_client())
        for _ in range(M):
            read_clients.append(new_read_client())

        for _ in tqdm(range(Q)):
            if random.random() < 0.3:
                i = random.randint(0, M - 1)
                if read_clients[i].invalid:
                    read_clients[i] = new_read_client()
                read_clients[i].random_walk()
            else:
                i = random.randint(0, N - 1)
                if write_clients[i].invalid:
                    write_clients[i] = new_write_client()
                write_clients[i].random_walk()
                write_clients[i].cleanup()
    finally:
        for server in write_servers + read_servers:
            server.kill()
        for conn in write_clients + read_clients:
            conn.close()


def testcase5write():
    N = 20
    M = 5
    Q = 3000
    print("(read/write client 5/20, read/write server 1/3, query 3000 (read:write 1:9), has reconnection)")

    try:
        generate_train_info()
        write_servers = [WriteServer(), WriteServer(), WriteServer()]
        read_servers = [ReadServer()]
        time.sleep(1)
        write_clients: list[WriteClient] = []
        read_clients: list[ReadClient] = []

        def new_write_client():
            return WriteClient(write_servers[random.randint(0, 2)].port)

        def new_read_client():
            return ReadClient(read_servers[0].port)

        for _ in range(N):
            write_clients.append(new_write_client())
        for _ in range(M):
            read_clients.append(new_read_client())

        for _ in tqdm(range(Q)):
            if random.random() < 0.1:
                i = random.randint(0, M - 1)
                if read_clients[i].invalid:
                    read_clients[i] = new_read_client()
                read_clients[i].random_walk()
            else:
                i = random.randint(0, N - 1)
                if write_clients[i].invalid:
                    write_clients[i] = new_write_client()
                write_clients[i].random_walk()
                write_clients[i].cleanup()
    finally:
        for server in write_servers + read_servers:
            server.kill()
        for conn in write_clients + read_clients:
            conn.close()


def testcase6read():
    N = 2
    Q = 100
    print("(read client 2, query 100, has reconnection, all busted)")

    try:
        generate_train_info()
        server = ReadServer()
        time.sleep(2)
        client: list[ReadClient] = []
        for _ in range(N):
            client.append(ReadClient(server.port, True))

        for _ in tqdm(range(Q)):
            i = random.randint(0, N - 1)
            client[i].random_walk()
            if client[i].invalid:
                client[i] = ReadClient(server.port, True)
    finally:
        server.kill()
        for conn in client:
            conn.close()


def testcase6write():
    N = 2
    Q = 100
    print("(write client 2, query 100, has reconnection, all busted)")

    try:
        generate_train_info()
        server = WriteServer()
        time.sleep(1)
        client: list[WriteClient] = []
        for _ in range(N):
            client.append(WriteClient(server.port, True))

        for _ in tqdm(range(Q)):
            i = random.randint(0, N - 1)
            client[i].random_walk()
            client[i].cleanup()
            if client[i].invalid:
                client[i] = WriteClient(server.port, True)
    finally:
        server.kill()
        for conn in client:
            conn.close()


def testcase6some():
    N = 50
    M = 10
    Q = 1000
    print("(read/write client 10/50, read/write server 1/3, query 1000 (read:write 3:7), has reconnection, 10% of client are busted)")
    try:
        generate_train_info()
        write_servers = [WriteServer(), WriteServer(), WriteServer()]
        read_servers = [ReadServer()]
        time.sleep(1)
        write_clients: list[WriteClient] = []
        read_clients: list[ReadClient] = []

        def new_write_client():
            return WriteClient(write_servers[random.randint(0, 2)].port, random.random() <= 0.1)

        def new_read_client():
            return ReadClient(read_servers[0].port, random.random() <= 0.1)

        for _ in range(N):
            write_clients.append(new_write_client())
        for _ in range(M):
            read_clients.append(new_read_client())

        for _ in tqdm(range(Q)):
            if random.random() < 0.3:
                i = random.randint(0, M - 1)
                if read_clients[i].invalid:
                    read_clients[i] = new_read_client()
                read_clients[i].random_walk()
            else:
                i = random.randint(0, N - 1)
                if write_clients[i].invalid:
                    write_clients[i] = new_write_client()
                write_clients[i].random_walk()
                write_clients[i].cleanup()
    finally:
        for server in write_servers + read_servers:
            server.kill()
        for conn in write_clients + read_clients:
            conn.close()


def testcase6all():
    N = 50
    M = 10
    Q = 1000
    print("(read/write client 10/50, read/write server 1/3, query 1000 (read:write 3:7), has reconnection, all clients are busted)")
    try:
        generate_train_info()
        write_servers = [WriteServer(), WriteServer(), WriteServer()]
        read_servers = [ReadServer()]
        time.sleep(1)
        write_clients: list[WriteClient] = []
        read_clients: list[ReadClient] = []

        def new_write_client():
            return WriteClient(write_servers[random.randint(0, 2)].port, True)

        def new_read_client():
            return ReadClient(read_servers[0].port, True)

        for _ in range(N):
            write_clients.append(new_write_client())
        for _ in range(M):
            read_clients.append(new_read_client())

        for _ in tqdm(range(Q)):
            if random.random() < 0.3:
                i = random.randint(0, M - 1)
                if read_clients[i].invalid:
                    read_clients[i] = new_read_client()
                read_clients[i].random_walk()
            else:
                i = random.randint(0, N - 1)
                if write_clients[i].invalid:
                    write_clients[i] = new_write_client()
                write_clients[i].random_walk()
                write_clients[i].cleanup()
    finally:
        for server in write_servers + read_servers:
            server.kill()
        for conn in write_clients + read_clients:
            conn.close()


yellow("* `make` before you test.\n")

time.sleep(2)

yellow("Disclaimer:")
print("\tDo note that this toolkit is only made for stress testing.")
print("\tInvaid input and error handling may not be covered thoroughly by this tool.")
print("\tAlso, the referenced behaviour (outputs) may not be correct.")
print("\tWe are welcome for any improvement of this tool.")

yellow("Do note that:")
print("- Every testcase below is \33[1mrandomly generated\33[0m every time. You may set fixed seeds for reproducible output.")
print("- Every testcase below \33[1mchecks timeout of ~5 sec\33[0m. If you wish to not test this, change MAX_CONN_TIME to a very large number.")
print("- Every testcase below has clients that \33[1mmay exit at any stage\33[0m.")

red("Known Issues:")
print("- Sometimes the grader cannot resolve potential race conditions, and thus it will stuck.")
print("- If you are pretty sure that your code is correct but get wrong answer on every test, change TIMEOUT to a large number.")

print()


import os, argparse
os.makedirs('logs', exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('-c', '--clear', action='store_true')
parser.add_argument('-t', '--test', type=int, nargs='+', default=[4,5,6])
args = parser.parse_args()

os.system('make')
# ---------------------------------------------------------------------------------------------
if 4 in args.test:
    ACTUAL_TOLERANCE = 0
    green("\33[1m[[ Subtask 4 (1 server, N client) ]]")
    run(testcase4read, 4)
    run(testcase4write1, 4)
    run(testcase4write2, 4)
# ---------------------------------------------------------------------------------------------
if 5 in args.test:
    ACTUAL_TOLERANCE = RACE_TOLERANCE
    green("\33[1m[[ Subtask 5 (4 server, N client) ]]")
    yellow("Note:")
    print(f"\tThis subtask (and multiserver version of the next subtask) use a constant RACE_TOLERANCE. (currently: {RACE_TOLERANCE})")
    print("\tIf some potentially client that can alter the referenced answer may timeout within RACE_TOLERANCE ms, the grader will sleep until all potential race condition is resolved.")
    print("\tFeel free to adjust this constant. However, too small tolarance may be simply impossible.")
    run(testcase5stress, 5)
    run(testcase5write, 5)
# ---------------------------------------------------------------------------------------------
if 6 in args.test:
    ACTUAL_TOLERANCE = RACE_TOLERANCE
    green("\33[1m[[ Subtask 6 (4 server, N busted client) ]]")
    run(testcase6read, 6)
    run(testcase6write, 6)
    run(testcase6some, 6)
    run(testcase6all, 6)
# ---------------------------------------------------------------------------------------------
yellow("Remember to clean up the logs!")


if args.clear:
    os.system('rm -f ./logs/*.log')