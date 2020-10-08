Tlog Log Format
===============

Tlog logs terminal I/O split into messages of maximum configured size, using
single message format. The message format is JSON described by a [JSON
Schema](schema.json), and accompanied by an [Elasticsearch
mapping](mapping.json). It has mostly flat structure with the following
properties:

| Name      | Type                      | Description
| --------- | ------------------------- | ----------------------
| ver       | String                    | Format version ("2.1")
| host      | String                    | Recording host name
| rec       | String                    | String uniquely identifying
|           |                           | the recording on the host
| user      | String                    | Recorded user name
| term      | String                    | Terminal type
| session   | Unsigned integer > 0      | Audit session ID
| id        | Unsigned integer > 0      | ID of the message within recording
| pos       | Unsigned integer          | Message position in recording,
|           |                           | milliseconds
| time      | Double                    | Message timestamp, seconds and
|           |                           | milliseconds since the Epoch
| timing    | String                    | Distribution of events in time
| in_txt    | String                    | Input text with invalid characters
|           |                           | scrubbed
| in_bin    | Array of unsigned bytes   | Scrubbed invalid input characters
|           |                           | as an array of bytes
| out_txt   | String                    | Output text with invalid characters
|           |                           | scrubbed
| out_bin   | Array of unsigned bytes   | Scrubbed invalid output characters
|           |                           | as an array of bytes

The `ver` field stores the version of the message format as a string
representing two unsigned integer numbers: major and minor, separated by a
dot. If both the dot and the minor number are omitted, the minor number is
assumed to be zero.

Increases in major version number represent changes which are not
forward-compatible, such as renaming a field, or changing a field format,
which existing software versions cannot handle. Increases in minor version
number represent changes which are forward-compatible, such as adding a new
field, which would be ignored by existing software versions.

Each message in a single recording has the same `host`, `rec`, `user`, `term`,
and `session` property values. The `rec` value is an opaque string uniquely
identifying a particular recording on the recording host. The `id` value
starts with one for the first message in the recording and is incremented for
each new message. The `pos` value is the message's temporal position from the
start of recording, in milliseconds. The `time` value is a real clock
timestamp, formatted as seconds.milliseconds since the Epoch.

The `in_txt`/`out_txt` properties are optional, storing input and output
text respectively. These property values default to an empty text string if
absent. Any byte sequences which don't constitute a valid character are
represented by [Unicode replacement characters][replacement_character] in
those strings and are instead stored in `in_bin`/`out_bin` byte arrays.

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

Example
-------

The example message below captures a user pasting a "date" command into their
terminal, the command output, and a fresh shell prompt:

    {
        "ver":      "2.1",
        "host":     "server.example.com",
        "rec":      "e843f15839e54e7d83bdc8c128978586-22c2-5d24f15",
        "user":     "johndoe",
        "term":     "xterm",
        "session":  324,
        "id":       23,
        "pos":      345349,
        "time":     1600718060.667,
        "timing":   "=80x24<5+1>6+3>30+6>20",
        "in_txt":   "date\r",
        "in_bin":   [],
        "out_txt":  "date\r\nMon Nov 30 11:52:45 UTC 2015\r\n[johndoe@server ~]$ ",
        "out_bin":  []
    }

Changelog
---------

### 2.3 - 2020-09-21
#### Added
- Added wall clock timestamp field - "time".

### 2.2 - 2017-09-07
#### Added
- Added origin to the value of the "pos" field: start of recording.

### 2.1 - 2017-07-13
#### Added
- Added record ID field - "rec".

### 2 - 2017-07-13
#### Changed
- Changed "ver" field type to string.

### 1 - 2015-11-30
#### Added
- Added this documentation.
- Implemented logging.
#### Changed
- Changed the format many times during initial development.

[replacement_character]: https://en.wikipedia.org/wiki/Specials_%28Unicode_block%29#Replacement_character
[ABNF]: https://tools.ietf.org/html/rfc5234
