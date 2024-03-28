"""Configuration module used for integration tests"""
import json
from misc import mkcfgfile

DEFAULT_TLOG_REC_LATENCY = 10
DEFAULT_TLOG_REC_PAYLOAD = 2048
DEFAULT_TLOG_REC_LOG_INPUT = False
DEFAULT_TLOG_REC_LOG_OUTPUT = True
DEFAULT_TLOG_REC_LOG_WINDOW = True
DEFAULT_TLOG_REC_LIMIT_RATE = 16384
DEFAULT_TLOG_REC_LIMIT_BURST = 32768
DEFAULT_TLOG_REC_LIMIT_ACTION = "pass"
DEFAULT_TLOG_REC_WRITER = "file"
DEFAULT_TLOG_REC_JOURNAL_PRIORITY = "info"
DEFAULT_TLOG_REC_JOURNAL_AUGMENT = True
DEFAULT_TLOG_REC_SYSLOG_FACILITY = "authpriv"
DEFAULT_TLOG_REC_SYSLOG_PRIORITY = "info"

DEFAULT_TLOG_REC_SESSION_SHELL = "/bin/bash"
DEFAULT_TLOG_REC_SESSION_NOTICE = "ATTENTION: Your session is being recorded!"
DEFAULT_TLOG_REC_SESSION_WRITER = "journal"
DEFAULT_TLOG_REC_SESSION_SESSION_LOCKING = True

DEFAULT_TLOG_PLAY_READER = "file"
DEFAULT_TLOG_PLAY_PERSIST = False
DEFAULT_TLOG_PLAY_LAX = False


class TlogPlayConfig:
    """Tlog Play configuration class"""
    def __init__(self, reader=DEFAULT_TLOG_PLAY_READER,
                 persist=DEFAULT_TLOG_PLAY_PERSIST,
                 lax=DEFAULT_TLOG_PLAY_LAX, file_reader_path=None,
                 es_baseurl=None,
                 es_query=None):
        self.reader = reader
        self.persist = persist
        self.lax = lax
        self.file_reader_path = file_reader_path
        self.es_baseurl = es_baseurl
        self.es_query = es_query

    def _setup_base_play_config(self):
        # unconditional tlog-play configuration options
        base_config = {
            "persist": self.persist,
            "lax": self.lax
        }

        return base_config

    def _setup_reader_config(self):
        if self.reader == "file":
            if self.file_reader_path is None:
                raise ValueError("file_reader_path must be provided")
            reader_config = {
                "reader": "file",
                "file": {
                    "path": self.file_reader_path
                },
            }
        elif self.reader == "es":
            if self.es_baseurl is None or self.es_query is None:
                raise ValueError("es_baseurl and es_query must be provided")
            reader_config = {
                "reader": "es",
                "es": {
                    "baseurl": self.es_baseurl,
                    "query": self.es_query
                },
            }
        else:
            raise ValueError("Unknown reader")

        return reader_config

    def _setup_config(self):
        config = self._setup_base_play_config()
        # append the reader configuration to the base configuration
        reader_config = self._setup_reader_config()
        config.update(reader_config)
        return config

    def generate_config(self, filename):
        """ Generate a configuration and write it to filename """
        tlog_play_config = self._setup_config()
        mkcfgfile(filename, json.dumps(tlog_play_config, indent=4))


class TlogRecConfig:
    """Tlog Rec configuration class"""
    def __init__(self, latency=DEFAULT_TLOG_REC_LATENCY,
                 payload=DEFAULT_TLOG_REC_PAYLOAD,
                 log_input=DEFAULT_TLOG_REC_LOG_INPUT,
                 log_output=DEFAULT_TLOG_REC_LOG_OUTPUT,
                 log_window=DEFAULT_TLOG_REC_LOG_WINDOW,
                 limit_rate=DEFAULT_TLOG_REC_LIMIT_RATE,
                 limit_burst=DEFAULT_TLOG_REC_LIMIT_BURST,
                 limit_action=DEFAULT_TLOG_REC_LIMIT_ACTION,
                 writer=DEFAULT_TLOG_REC_WRITER, file_writer_path=None,
                 journal_priority=DEFAULT_TLOG_REC_JOURNAL_PRIORITY,
                 journal_augment=DEFAULT_TLOG_REC_JOURNAL_AUGMENT,
                 syslog_facility=DEFAULT_TLOG_REC_SYSLOG_FACILITY,
                 syslog_priority=DEFAULT_TLOG_REC_SYSLOG_PRIORITY):
        self.latency = latency
        self.payload = payload
        self.log_input = log_input
        self.log_output = log_output
        self.log_window = log_window
        self.limit_rate = limit_rate
        self.limit_burst = limit_burst
        self.limit_action = limit_action
        self.writer = writer
        self.file_writer_path = file_writer_path
        self.journal_priority = journal_priority
        self.journal_augment = journal_augment
        self.syslog_facility = syslog_facility
        self.syslog_priority = syslog_priority

    def _setup_base_config(self):
        # unconditional tlog-rec configuration options
        base_config = {
            "latency": self.latency,
            "payload": self.payload,
            "log": {
                "input": self.log_input,
                "output": self.log_output,
                "window": self.log_window
            },
            "limit": {
                "rate": self.limit_rate,
                "burst": self.limit_burst,
                "action": self.limit_action
            },
        }

        return base_config

    def _setup_writer_config(self):
        # create the writer configuration based on input
        if self.writer == "file":
            if self.file_writer_path is None:
                raise ValueError("file_writer_path must be provided")
            writer_config = {
                "file": {
                    "path": self.file_writer_path
                },
                "writer": self.writer
            }
        elif self.writer == "journal":
            writer_config = {
                "journal": {
                    "priority": self.journal_priority,
                    "augment": self.journal_augment
                },
                "writer": self.writer
            }
        elif self.writer == "syslog":
            writer_config = {
                "syslog": {
                    "facility": self.syslog_facility,
                    "priority": self.syslog_priority
                },
                "writer": self.writer
            }
        else:
            raise ValueError("Unknown writer")

        return writer_config

    def _setup_config(self):
        config = self._setup_base_config()

        # append the writer configuration to the base configuration
        writer_config = self._setup_writer_config()
        config.update(writer_config)
        return config

    def generate_config(self, filename):
        """ Generate a configuration and write it to filename """

        tlog_rec_config = self._setup_config()
        mkcfgfile(filename, json.dumps(tlog_rec_config, indent=4))


class TlogRecSessionConfig(TlogRecConfig):
    """TlogPlaySession configuration class, child of TlogRecConfig"""
    def __init__(self, session_locking = DEFAULT_TLOG_REC_SESSION_SESSION_LOCKING,
                 shell=DEFAULT_TLOG_REC_SESSION_SHELL,
                 notice=DEFAULT_TLOG_REC_SESSION_NOTICE,
                 latency=DEFAULT_TLOG_REC_LATENCY,
                 payload=DEFAULT_TLOG_REC_PAYLOAD,
                 log_input=DEFAULT_TLOG_REC_LOG_INPUT,
                 log_output=DEFAULT_TLOG_REC_LOG_OUTPUT,
                 log_window=DEFAULT_TLOG_REC_LOG_WINDOW,
                 limit_rate=DEFAULT_TLOG_REC_LIMIT_RATE,
                 limit_burst=DEFAULT_TLOG_REC_LIMIT_BURST,
                 limit_action=DEFAULT_TLOG_REC_LIMIT_ACTION,
                 writer=DEFAULT_TLOG_REC_SESSION_WRITER, file_writer_path=None,
                 journal_priority=DEFAULT_TLOG_REC_JOURNAL_PRIORITY,
                 journal_augment=DEFAULT_TLOG_REC_JOURNAL_AUGMENT,
                 syslog_facility=DEFAULT_TLOG_REC_SYSLOG_FACILITY,
                 syslog_priority=DEFAULT_TLOG_REC_SYSLOG_PRIORITY):
        self.shell = shell
        self.notice = notice
        self.session_locking = session_locking
        super().__init__(latency, payload, log_input, log_output, log_window,
                         limit_rate, limit_burst, limit_action,
                         writer, file_writer_path, journal_priority,
                         journal_augment, syslog_facility,
                         syslog_priority)

    def _setup_base_session_config(self):
        # unconditional tlog-rec-session configuration options
        tlog_rec_session_config = {
            "shell": self.shell,
            "notice": self.notice,
            "session_locking": self.session_locking
        }

        return tlog_rec_session_config

    def generate_config(self, filename):
        """ Generate a configuration and write it to filename """

        # append TlogRecConfig configuration to our base configuration
        tlog_rec_session_config = self._setup_base_session_config()
        tlog_rec_config = super()._setup_config()
        tlog_rec_session_config.update(tlog_rec_config)

        mkcfgfile(filename, json.dumps(tlog_rec_session_config, indent=4))
