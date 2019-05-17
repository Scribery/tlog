import select
import pexpect

from systemd import journal

def journal_event_poll(journal_reader, timeout):
    p = select.poll()
    p.register(journal_reader, journal_reader.get_events())
    timeout_ms = timeout * 1000
    ret = p.poll(timeout_ms)
    if ret:
        return journal_reader

def find_journal_entry_with_match(pattern, match_filter, timeout):
    reader = journal.Reader()
    reader.this_boot()
    reader.seek_tail()
    reader.add_match(match_filter)

    log_entry = journal_event_poll(reader, timeout)
    if log_entry is None:
        raise TimeoutError("No matching journal event found")
    else:
        for entry in log_entry:
            message = entry['MESSAGE']
            assert pattern in message
