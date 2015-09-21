import multiprocessing
import time


def _io_bound_work():
    time.sleep(10.0)  # to simulate i/o bound work


class TestProcess(multiprocessing.Process):
    def run(self):
        _io_bound_work()


if __name__ == '__main__':
    for _ in xrange(8):
        process = TestProcess()
        process.start()
    for process in multiprocessing.active_children():
        process.join()
