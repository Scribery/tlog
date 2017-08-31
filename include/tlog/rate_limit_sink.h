#ifndef _TLOG_RATE_LIMIT_SINK_H
#define _TLOG_RATE_LIMIT_SINK_H

#include <tlog/sink.h>

/** Rate Limit sink type */
extern const struct tlog_sink_type tlog_rate_limit_sink_type;


/**
 * Create (allocate and initialize) a log sink.
 *
 * @param psink     Location for created sink pointer, set to NULL
 *                  in case of error.
 * @param dsink     Location of the destination/log sink to write to.
 * @param rate      Maximum cutoff rate for writing to the log.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_rate_limit_sink_create(struct tlog_sink **psink,
                            struct tlog_sink  *dsink,
                            bool               dsink_owned,
                            uint64_t           rate,
                            struct tlog_pkt   *pkt)
{
    assert(psink != NULL);
    assert(tlog_sink_is_valid(dsink));
    return tlog_sink_create(psink, &tlog_rate_limit_sink_type,
                            dsink, dsink_owned, rate, pkt);
}

#endif /* _TLOG_RATE_LIMIT_SINK_H */
