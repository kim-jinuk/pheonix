using System;

public sealed class LoggerEntry
{
    public LoggerEntry(DateTime time, byte cmdFlag, string cmdText, bool isAck = false)
    {
        Time = time;
        CmdFlag = cmdFlag;
        CmdText = cmdText;
        IsAck = isAck;
    }

    public DateTime Time { get; }
    public byte CmdFlag { get; }
    public string CmdText { get; }
    public bool IsAck { get; }

    public override string ToString()
    {
        string timestamp = Time.ToString("HH:mm:ss.fff");
        string prefix = IsAck ? "[ACK]" : "[CMD]";
        return $"{timestamp} {prefix} {CmdText}";
    }
}
