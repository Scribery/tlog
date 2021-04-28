""" tlog tests """
import os
import stat
from tempfile import mkdtemp

from misc import ssh_pexpect, \
                 mklogfile, check_recording, \
                 check_recording_missing, check_outfile


class TestTlogRecPerformanceOptions:
    """ Test performance related options for tlog-rec """
    user1 = 'tlitestlocaluser1'
    tempdir = mkdtemp(prefix='/tmp/TestTlogRec.')
    os.chmod(tempdir, stat.S_IRWXU + stat.S_IRWXG + stat.S_IRWXO +
             stat.S_ISUID + stat.S_ISGID + stat.S_ISVTX)

    def test_record_fast_input_with_latency(self):
        """
        Check tlog-rec caches data some time before logging
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        opts = '--latency=9'
        shell.sendline('tlog-rec {} '
                       '-o {} /bin/bash'.format(opts, logfile))
        for num in range(0, 200):
            shell.sendline('echo test_{}'.format(num))
        shell.sendline('exit')
        check_recording(shell, 'test_199', logfile)
        shell.close()

    def test_record_fast_input_with_payload(self):
        """
        Check tlog-rec limits output payload size
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        opts = '--payload=128'
        shell.sendline('tlog-rec {} '
                       '-o {} /bin/bash'.format(opts, logfile))
        for num in range(0, 200):
            shell.sendline('echo test_{}'.format(num))
        shell.sendline('exit')
        check_recording(shell, 'test_199', logfile)
        shell.close()

    def test_record_fast_input_with_limit_rate(self):
        """
        Check tlog-rec records session with limit-rate argument
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        opts = '--limit-rate=10'
        shell.sendline('tlog-rec {} '
                       '-o {} /bin/bash'.format(opts, logfile))
        for num in range(0, 200):
            shell.sendline('echo test_{}'.format(num))
        shell.sendline('exit')
        shell.close()

    def test_record_fast_input_with_limit_burst(self):
        """
        Check tlog-rec allows limited burst of fast output
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        opts = '--limit-rate=10 --limit-burst=100'
        shell.sendline('tlog-rec {} '
                       '-o {} /bin/bash'.format(opts, logfile))
        for num in range(0, 200):
            shell.sendline('echo test_{}'.format(num))
        shell.sendline('exit')
        shell.close()

    def test_record_fast_input_with_limit_action_drop(self):
        """
        Check tlog-rec drops output when logging limit reached
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        opts = '--limit-rate=10 --limit-action=drop'
        cmd = 'cat /usr/share/dict/linux.words'
        shell.sendline('tlog-rec {} '
                       '-o {} {}'.format(opts, logfile, cmd))
        shell.close()
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        check_recording_missing(shell, 'Byronite', logfile)
        check_recording_missing(shell, 'zygote', logfile)

    def test_record_fast_input_with_limit_action_delay(self):
        """
        Check tlog-rec delays recording when logging limit reached
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        opts = '--limit-rate=10 --limit-action=delay'
        shell.sendline('tlog-rec {} '
                       '-o {} /bin/bash'.format(opts, logfile))
        for num in range(0, 200):
            shell.sendline('echo test_{}'.format(num))
        check_outfile('test_199', logfile, maxchecks=100)
        shell.sendline('exit')
        check_recording(shell, 'test_199', logfile)
        shell.close()

    def test_record_fast_input_with_limit_action_pass(self):
        """
        Check tlog-rec ignores logging limits
        """
        logfile = mklogfile(self.tempdir)
        shell = ssh_pexpect(self.user1, 'Secret123', 'localhost')
        opts = '--limit-rate=10 --limit-action=pass'
        shell.sendline('tlog-rec {} '
                       '-o {} /bin/bash'.format(opts, logfile))
        for num in range(0, 200):
            shell.sendline('echo test_{}'.format(num))
        shell.sendline('exit')
        check_recording(shell, 'test_199', logfile)
        shell.close()
