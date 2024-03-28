""" tlog tests """
import os
import stat
import time
import inspect
from tempfile import mkdtemp
from subprocess import Popen, PIPE, STDOUT
import pytest

from misc import check_recording, mklogfile, mkcfgfile, \
                 ssh_pexpect, check_recording_missing, copyfile
from config import TlogRecConfig, TlogRecSessionConfig

TLOG_REC_SESSION_PROG = "/usr/bin/tlog-rec-session"
SYSTEM_TLOG_REC_SESSION_CONF = "/etc/tlog/tlog-rec-session.conf"

@pytest.fixture
def utempter_enabled():
    p = Popen(['ldd', TLOG_REC_SESSION_PROG],
              stdout=PIPE, stdin=PIPE, stderr=PIPE, encoding='utf8')
    stdout_data = p.communicate()[0]
    return 'libutempter.so' in stdout_data


class TestTlogRecSession:
    """ Test tlog-rec-session functionality """
    user = 'tlitestlocaluser2'
    tempdir = mkdtemp(prefix='/tmp/TestRecSession.')
    os.chmod(tempdir, stat.S_IRWXU + stat.S_IRWXG + stat.S_IRWXO +
             stat.S_ISUID + stat.S_ISGID + stat.S_ISVTX)

    @pytest.mark.tier1
    def test_session_record_to_file_locking_enabled(self):
        """
        Check multiple recordings in a session only records one at a time (default)
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file", file_writer_path=logfile)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('echo {}_shell0'.format(myname))
        shell.sendline('stty -echo')
        shell.sendline("tlog-rec-session -c 'echo {}_nested_session' >/dev/null".format(myname))
        shell.sendline('exit')
        check_recording(shell, "{}_shell0".format(myname), logfile)
        check_recording_missing(shell, "{}_nested_session".format(myname), logfile)
        shell.close()

    @pytest.mark.tier1
    def test_session_record_to_file_locking_disabled(self):
        """
        Check multiple recordings in a session works in tlog-rec-session with locking-enabled setting
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file", file_writer_path=logfile, session_locking=False)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('echo {}_shell0'.format(myname))
        shell.sendline('stty -echo')
        shell.sendline("tlog-rec-session -c 'echo {}_nested_session' >/dev/null".format(myname))
        shell.sendline('exit')
        check_recording(shell, "{}_shell0".format(myname))
        check_recording(shell, "{}_nested_session".format(myname))
        shell.close()

    @pytest.mark.tier1
    def test_session_record_to_file(self):
        """
        Check tlog-rec-session preserves session in a file
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('echo {}'.format(myname))
        shell.sendline('exit')
        check_recording(shell, myname, logfile)
        shell.close()

    @pytest.mark.tier1
    def test_session_record_to_journal(self):
        """
        Check tlog-rec-session preserves session in journal
        """
        myname = inspect.stack()[0][3]
        sessionclass = TlogRecSessionConfig(writer="journal")
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('echo {}'.format(myname))
        shell.sendline('exit')
        check_recording(shell, myname)
        shell.close()

    @pytest.mark.tier1
    def test_session_record_to_syslog(self):
        """
        Check tlog-rec-session preserves session via syslog
        """
        myname = inspect.stack()[0][3]
        sessionclass = TlogRecSessionConfig(writer="syslog")
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('echo {}'.format(myname))
        shell.sendline('exit')
        check_recording(shell, myname)
        shell.close()

    def test_session_record_fast_input_with_latency(self):
        """
        Check tlog-rec-session caches data some time before logging
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            latency=15)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_fast_input_with_payload(self):
        """
        Check tlog-rec limits output payload size
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            payload=128)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_fast_input_with_limit_rate(self):
        """
        Check tlog-rec-session records session with limit rate
        configured
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            limit_rate=10)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_fast_input_with_limit_burst(self):
        """
        Check tlog-rec-session allows limited burst of fast output
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            limit_rate=10,
                                            limit_burst=100)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_fast_input_with_limit_action_drop(self):
        """
        Check tlog-rec-session drops output when logging limit reached
        """
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            limit_rate=10,
                                            limit_action="drop")
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('cat /usr/share/dict/linux.words')
        time.sleep(1)
        shell.sendline('exit')
        shell.close()
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        check_recording_missing(shell, 'Byronite', logfile)
        check_recording_missing(shell, 'zygote', logfile)

    def test_session_record_fast_input_with_limit_action_delay(self):
        """
        Check tlog-rec-session delays recording when logging limit reached
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            limit_rate=500,
                                            limit_action="delay")
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_fast_input_with_limit_action_pass(self):
        """
        Check tlog-rec-session ignores logging limits
        """
        myname = inspect.stack()[0][3]
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile,
                                            limit_rate=500,
                                            limit_action="pass")
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        for num in range(0, 200):
            shell.sendline('echo {}_{}'.format(myname, num))
        shell.sendline('exit')
        check_recording(shell, '{}_199'.format(myname), logfile)
        shell.close()

    def test_session_record_with_different_shell(self):
        """
        Check tlog-rec-session can specify different shell
        """
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(shell="/usr/bin/tcsh",
                                            writer="file",
                                            file_writer_path=logfile)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('echo $SHELL')
        check_recording(shell, '/usr/bin/tcsh', logfile)
        shell.sendline('exit')

    def test_session_record_pipe_io_stdin(self):
        """
        Pipe I/O through stdin
        """
        text_in_stdio = 'print("hello world")\n'
        text_out = "hello world"
        p = Popen(['sshpass', '-p', 'Secret123', 'ssh', '-o',
                   'StrictHostKeyChecking=no',
                   'tlitestlocaluser2@localhost', 'python3'],
        stdout=PIPE, stdin=PIPE, stderr=PIPE, encoding='utf8')
        stdout_data = p.communicate(input=text_in_stdio)[0]
        assert text_out in stdout_data

    def test_session_record_user_in_utmp(self, utempter_enabled):
        """
        Check tlog-rec-session preserves session in a file
        """
        if not utempter_enabled:
            pytest.skip('utempter not enabled, skipping test')
        myname = inspect.stack()[0][3]
        whoami = '{} pts'.format(self.user)
        logfile = mklogfile(self.tempdir)
        sessionclass = TlogRecSessionConfig(writer="file",
                                            file_writer_path=logfile)
        sessionclass.generate_config(SYSTEM_TLOG_REC_SESSION_CONF)
        shell = ssh_pexpect(self.user, 'Secret123', 'localhost')
        shell.sendline('echo {}'.format(myname))
        shell.sendline('who am i')
        shell.sendline('exit')
        check_recording(shell, myname, logfile)
        check_recording(shell, whoami, logfile)
        shell.close()

    @classmethod
    def teardown_class(cls):
        """ Copy original conf file back into place """
        filename = SYSTEM_TLOG_REC_SESSION_CONF
        bkup = '{}.origtest'.format(filename)
        copyfile(bkup, filename)
