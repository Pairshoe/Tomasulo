import web, json, sys

urls = (
    "/", "Home",
    "/run", "Run",
    "/reset", "Reset",
)


INPUT_PATH = sys.argv[2]
STATE_PATH = './state.txt'


class Tomasulo:
    def __init__(self):
        self.code = []
        self.instr = []
        with open(INPUT_PATH, 'r') as f:
            self.code = f.readlines()
        with open(STATE_PATH, 'r') as f:
            lines = f.readlines()
            for line in lines:
                if line.startswith('instr='):
                    self.instr += [line.split('=')[1].rstrip('\n')]
        self.cycle = 0

    def getState(self, step: int):
        states = {'code': self.code, 'instr': self.instr, 'cycle': self.cycle, 'done': False}
        with open(STATE_PATH, 'r') as f:
            total = int(f.readlines()[-1])
            print(total)
        if step == 1:
            self.cycle += 1
        elif step == 0:
            return states
        elif step == -1:
            if self.cycle != 0:
                self.cycle -= 1
            else:
                return states
        else:
            self.cycle = total
        states['cycle'] = self.cycle
        if self.cycle == total:
            states['done'] = True
        with open(STATE_PATH, 'r') as f:
            lines = f.readlines()
            start = lines.index('Cycle=' + str(self.cycle) + '\n')
            end = lines.index('Cycle=' + str(self.cycle + 1) + '\n') if self.cycle != total else lines.index(str(total))
            for line in lines[start+1:end]:
                entry = line.split('=')
                states[entry[0]] = entry[1].rstrip('\n')
        return states

    def clear(self):
        self.cycle = 0


tmsl = Tomasulo()


class Home:
    def GET(self):
        raise web.seeother("/static/index.html")


class Run:
    def POST(self):
        step = json.loads(web.data().decode())['step']
        return json.dumps(tmsl.getState(step))


class Reset:
    def POST(self):
        tmsl.clear()
        return json.dumps(tmsl.getState(0))


if __name__ == "__main__":
    web.application(urls, globals()).run()
