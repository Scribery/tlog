Tlog Log Format
===============

Tlog logs terminal I/O split into messages of maximum configured size, using
single message format. The message format is JSON and is described by a [JSON
Schema](schema.json). It has mostly flat structure with the following
properties:

| Name      | Type                      | Description
| --------- | ------------------------- | ----------------------
| host      | String                    | Recording host name
| user      | String                    | Recorded user name
| session   | String                    | Recorded audit session ID
| id        | Unsigned integer          | ID of the message within session
| pos       | Unsigned integer          | Message position in session,
|           |                           | milliseconds
| timing    | String                    | Distribution of events in time
| in_txt    | String                    | Input text with invalid characters
|           |                           | scrubbed
| in_bin    | Array of unsigned bytes   | Scrubbed invalid input characters
|           |                           | as an array of bytes
| out_txt   | String                    | Output text with invalid characters
|           |                           | scrubbed
| out_bin   | Array of unsigned bytes   | Scrubbed invalid output characters
|           |                           | as an array of bytes

Each message in a single session has the same `host`, `user`, and `session`
property values. The `id` value starts with one for the first message and is
incremented for each new message. The `pos` value is the message temporal
position in a session, in milliseconds. No assumption should be made about the
time origin at this moment.

The `in_txt`/`out_txt` properties store input and output text respectively.
Any byte sequences which don't constitute a valid character are represented by
[Unicode replacement characters][replacement_character] in those strings and
are instead stored in `in_bin`/`out_bin` byte arrays.

The `timing` value describes how much input and output was done and how
terminal window size changed at which time offset since the time stored in
`pos`.  The `timing` value format can be described with the following
[ABNF][ABNF] rule set:

    timing  = *([delay] record)

    delay   = "+" 1*DIGIT   ; Delay of the next record since the "pos" value,
                            ; or since the previous record, milliseconds.

    record  = in-txt / in-bin / out-txt / out-bin / window

    in-txt  = "<" 1*DIGIT   ; The number of input characters to take next
                            ; from the "in_txt" string.

    in-bin  = "[" 1*DIGIT "/" 1*DIGIT   ; The number of replacement input
                                        ; characters to skip next in the
                                        ; "in_txt" string (first number),
                                        ; plus the number of input bytes to
                                        ; take next from the "in_bin" array
                                        ; (second number).

    out-txt  = ">" 1*DIGIT  ; The number of output characters to take next
                            ; from the "out_txt" string.

    out-bin  = "]" 1*DIGIT "/" 1*DIGIT  ; The number of replacement output
                                        ; characters to skip next in the
                                        ; "out_txt" string (first number),
                                        ; plus the number of output bytes to
                                        ; take next from the "out_bin" array
                                        ; (second number).

    window  = "=" 1*DIGIT "x" 1*DIGIT   ; New window width (columns, first
                                        ; number) and height (rows, second
                                        ; number).

The example message below captures a user pasting a "date" command into their
terminal, the command output, and a fresh shell prompt:

    {
        "host":     "server.example.com",
        "user":     "johndoe",
        "session":  324,
        "id":       23,
        "pos":      345349,
        "timing":   "=80x24<5+1>6+3>30+6>20",
        "in_txt":   "date\r",
        "in_bin":   [],
        "out_txt":  "date\r\nMon Nov 30 11:52:45 UTC 2015\r\n[johndoe@server ~]$ ",
        "out_bin":  []
    }

[replacement_character]: https://en.wikipedia.org/wiki/Specials_%28Unicode_block%29#Replacement_character
[ABNF]: https://tools.ietf.org/html/rfc5234
