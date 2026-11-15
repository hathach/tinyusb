"""The real board lock of a stubbed usbtest rig: run_case.py's hil_lock in a temp lock dir and its
busy check stubbed, read back by the tests: released, cleared, the cleanup log's events."""
import os
import tempfile
from unittest import mock


class RigLock:
    def __init__(self, test, hil_lock, hil_flash, on_take=lambda name: None):
        td = tempfile.TemporaryDirectory()
        test.addCleanup(td.cleanup)
        self.hil_lock, self.on_take = hil_lock, on_take
        self.users = []     # what the busy check names; an exception raises from it
        self.error = None   # raised by the take instead: an exception, or the message of a RuntimeError
        self.name = None    # the lock name of the last take
        real_take = self.real_take = hil_lock.take_board_lock

        def take(board, reason, **kw):
            if isinstance(self.error, BaseException):
                raise self.error
            if self.error:
                raise RuntimeError(self.error)
            self.name = board
            self.on_take(board)
            return real_take(board, reason, **kw)
        for obj, name, value in ((hil_lock, 'BOARD_LOCK_DIR', td.name), (hil_lock, 'take_board_lock', take),
                                 (hil_flash, 'probe_busy', self.probe_busy)):
            patcher = mock.patch.object(obj, name, value)
            patcher.start()
            test.addCleanup(patcher.stop)
        env = mock.patch.dict(os.environ)
        env.start()
        test.addCleanup(env.stop)
        os.environ.pop('HIL_NO_BOARD_LOCK', None)

    def probe_busy(self, board):
        if isinstance(self.users, BaseException):
            raise self.users
        return list(self.users)

    @property
    def cleared(self):
        return self.name is not None and self.hil_lock.record_state(self.name)[0] != 'record'

    @property
    def closed(self):
        """A peer can take the lock (read after `cleared`: a peer's take logs a record left behind)."""
        try:
            fh = self.real_take(self.name, 'peer')
        except self.hil_lock.BoardLocked:
            return False
        self.hil_lock.release_board_lock(fh)
        return True

    def events(self, kind):
        return [e for e in self.hil_lock.cleanup_events(self.name) if e['event'] == kind]
