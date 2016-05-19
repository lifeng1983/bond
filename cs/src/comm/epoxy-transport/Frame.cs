﻿// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

namespace Bond.Comm.Epoxy
{
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.Threading;
    using System.Threading.Tasks;
    using System.Diagnostics;

    internal enum FrameletType
    {
        EpoxyConfig = 0x4743,
        EpoxyHeaders = 0x5248,
        LayerData = 0x594C,
        PayloadData = 0x5444,
        ProtocolError = 0x5245,
    }

    internal struct Framelet
    {
        private FrameletType m_type;
        private ArraySegment<byte> m_content;

        public Framelet(FrameletType type, ArraySegment<byte> contents)
        {
            switch (type)
            {
                case FrameletType.EpoxyConfig:
                case FrameletType.EpoxyHeaders:
                case FrameletType.LayerData:
                case FrameletType.PayloadData:
                case FrameletType.ProtocolError:
                    break;

                default:
                    throw new ArgumentException("Unknown framelet type", nameof(type));
            }

            if (contents.Array == null || contents.Count == 0)
            {
                throw new ArgumentException("Framelet contents cannot be empty", nameof(contents));
            }

            m_type = type;
            m_content = contents;
        }

        public static bool IsKnownType(UInt16 value)
        {
            return value == (UInt16)FrameletType.LayerData
                | value == (UInt16)FrameletType.PayloadData
                | value == (UInt16)FrameletType.EpoxyHeaders
                | value == (UInt16)FrameletType.EpoxyConfig
                | value == (UInt16)FrameletType.ProtocolError;
        }

        public FrameletType Type { get { return m_type; } }
        public ArraySegment<byte> Contents { get { return m_content; } }
    }

    internal class Frame
    {
        // most frames will have at most three framelets: EpoxyHeaders, LayerData, PayloadData
        private const int DefaultFrameCount = 3;

        private readonly List<Framelet> m_framelets;

        public Frame() : this(DefaultFrameCount) { }

        public Frame(int capacity)
        {
            m_framelets = new List<Framelet>(capacity);
        }

        public IReadOnlyList<Framelet> Framelets { get { return m_framelets; } }

        public int Count { get { return m_framelets.Count; } }

        public void Add(Framelet framelet)
        {
            if (m_framelets.Count == UInt16.MaxValue)
            {
                var message = LogUtil.ErrorAndReturnFormatted("{0}.{1}: Exceeded maximum allowed count of framelets.",
                    nameof(Frame), nameof(Add));
                throw new InvalidOperationException(message);
            }

            m_framelets.Add(framelet);
        }

        public void Write(BinaryWriter dest)
        {
            if (dest == null)
            {
                throw new ArgumentNullException(nameof(dest));
            }

            if (m_framelets.Count == 0)
            {
                throw new InvalidOperationException("Must have at least one framelet to write a frame.");
            }

            // Add ensures that we never have more than UInt16 framelets
            var numFramelets = unchecked((UInt16)m_framelets.Count);
            dest.Write(numFramelets);

            var frameletsWritten = 0;
            try
            {
                foreach (var framelet in m_framelets)
                {
                    // Framelet ctor checks that the type is valid, so we don't need to worry about overflow
                    var frameletType = unchecked((UInt16)framelet.Type);
                    // ArraySegment checks that the Count is not negative, so we don't need to worry about overflow
                    var frameletLength = unchecked((UInt32)framelet.Contents.Count);

                    dest.Write(frameletType);
                    dest.Write(frameletLength);
                    dest.Write(framelet.Contents.Array, framelet.Contents.Offset, framelet.Contents.Count);
                    frameletsWritten++;
                }
            }
            catch (Exception ex) when (ex is IOException || ex is ObjectDisposedException)
            {
                Log.Error(ex, "{0}.{1}: Only wrote {2} of {3} framelets!", nameof(Frame), nameof(Write), frameletsWritten, numFramelets);
                throw;
            }
        }

        public static async Task<Frame> ReadAsync(Stream stream, CancellationToken ct)
        {
            try
            {
                var frameletCount = await ReadUInt16Async(stream, ct);
                if (frameletCount == 0)
                {
                    throw new EpoxyProtocolErrorException("Zero framelets");
                }

                var frame = new Frame(frameletCount);

                while (frameletCount > 0)
                {
                    if (ct.IsCancellationRequested)
                    {
                        return null;
                    }

                    var frameletType = await ReadUInt16Async(stream, ct);
                    if (!Framelet.IsKnownType(frameletType))
                    {
                        throw new EpoxyProtocolErrorException("Unknown framelet type: " + frameletType);
                    }

                    var frameletLength = await ReadUInt32Async(stream, ct);
                    if (frameletLength > int.MaxValue)
                    {
                        throw new EpoxyProtocolErrorException("Framelet too big: " + frameletLength);
                    }

                    byte[] frameletContents = await ReadBufferAsync(stream, unchecked((int)frameletLength), ct);
                    frame.Add(new Framelet((FrameletType)frameletType, new ArraySegment<byte>(frameletContents)));

                    --frameletCount;
                }

                return frame;
            }
            catch (OperationCanceledException)
            {
                return null;
            }
        }

        static async Task<UInt16> ReadUInt16Async(Stream stream, CancellationToken ct)
        {
            var buf = await ReadBufferAsync(stream, sizeof(UInt16), ct);
            return BitConverter.ToUInt16(buf, 0);
        }

        static async Task<UInt32> ReadUInt32Async(Stream stream, CancellationToken ct)
        {
            var buf = await ReadBufferAsync(stream, sizeof(UInt32), ct);
            return BitConverter.ToUInt32(buf, 0);
        }

        static async Task<byte[]> ReadBufferAsync(Stream stream, int length, CancellationToken ct)
        {
            Debug.Assert(stream != null);
            Debug.Assert(length > 0);

            var buf = new byte[length];
            int bytesToRead = length;
            while (bytesToRead > 0)
            {
                ct.ThrowIfCancellationRequested();

                int dataRead = await stream.ReadAsync(buf, buf.Length - bytesToRead, bytesToRead, ct);
                if (dataRead == 0)
                {
                    throw new EndOfStreamException("End of stream encountered while reading fream");
                }

                bytesToRead -= dataRead;
            }

            return buf;
        }
    }
}
