/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "audio/decoders/opus.h"

#ifdef USE_OPUS

#include "common/debug.h"
#include "common/ptr.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "audio/audiostream.h"

#include <opusfile.h>

namespace Audio {

// These are wrapper functions to allow using a SeekableReadStream object to
// provide data to the OggOpusFile object.

static int read_stream_callback(void *_stream, unsigned char *_ptr, int _nbytes) {
	Common::SeekableReadStream *stream = (Common::SeekableReadStream *)_stream;
	uint32 result = stream->read(_ptr, _nbytes);
	return result;
}

static int seek_stream_callback(void *_stream, opus_int64 _offset, int _whence) {
	Common::SeekableReadStream *stream = (Common::SeekableReadStream *)_stream;
	return stream->seek((int32)_offset, _whence) ? 0 : -1;
}

static opus_int64 tell_stream_callback(void *_stream) {
	Common::SeekableReadStream *stream = (Common::SeekableReadStream *)_stream;
	return (opus_int64)stream->pos();
}

static int close_stream_callback(void *_stream) {
	// Do nothing -- we leave it up to the OpusStream to free memory as appropriate.
	return 0;
}

static OpusFileCallbacks g_stream_callbacks = {
	read_stream_callback,
	seek_stream_callback,
	tell_stream_callback,
	close_stream_callback
};

static const char *opusError(int errnum) {
	switch (errnum) {
		case OP_FALSE:         return "Request did not succeed";
		case OP_HOLE:          return "There was a hole in the data and some samples may have been skipped";
		case OP_EREAD:         return "An underlying read, seek, or tell operation failed";
		case OP_EFAULT:        return "Internal memory allocation or library error";
		case OP_EIMPL:         return "Unimplemented feature used in stream";
		case OP_EINVAL:        return "One or more parameters to a function were invalid";
		case OP_ENOTFORMAT:    return "Invalid Ogg Opus stream";
		case OP_EBADHEADER:    return "Invalid Ogg Opus stream";
		case OP_EVERSION:      return "Unrecognized version number in header";
		case OP_ENOTAUDIO:     return "Unknown error"; // This is unused as of opusfile-0.7
		case OP_EBADPACKET:    return "Failed to decode audio packet";
		case OP_EBADLINK:      return "Seeking error";
		case OP_ENOSEEK:       return "Non-seekable stream";
		case OP_EBADTIMESTAMP: return "Validity checks failed for first or last timestamp in a link";
	}
	return "Unknown error";
}


#pragma mark -
#pragma mark --- Ogg Opus stream ---
#pragma mark -


class OpusStream : public SeekableAudioStream {
protected:
	Common::DisposablePtr<Common::SeekableReadStream> _inStream;

	byte _channels;
	int _rate;
	Timestamp _length;
	OggOpusFile *_file;

	opus_int16 _buffer[120*48*2]; // recommended buffer size for 2 channels (120ms, 48kHz)
	const opus_int16 *_bufferEnd;
	const opus_int16 *_pos;

public:
	OpusStream(Common::SeekableReadStream *inStream, DisposeAfterUse::Flag dispose);
	~OpusStream();

	int readBuffer(int16 *buffer, const int numSamples);
	bool seek(const Timestamp &where);
	bool endOfData() const { return _pos >= _bufferEnd; }
	bool isStereo() const { return _channels >= 2; }
	int  getRate() const { return _rate; }
	Timestamp getLength() const { return _length; }

protected:
	bool _fillBuffer();
};

OpusStream::OpusStream(Common::SeekableReadStream *inStream, DisposeAfterUse::Flag dispose) :
	_inStream(inStream, dispose),
	_length(0, 1000),
	_bufferEnd(ARRAYEND(_buffer)) {

	int result = 0;
	_file = op_open_callbacks(inStream, &g_stream_callbacks, NULL, 0, &result);
	if (result < 0) {
		warning("Could not create Opus stream: %s", opusError(result));
		_pos = _bufferEnd;
		return;
	}

	_channels = op_channel_count(_file, -1);
	/*
		"All Opus audio is coded at 48 kHz, and should also be decoded at 48 kHz for playback
		(unless the target hardware does not support this sampling rate)."
		See https://wiki.xiph.org/OpusFAQ#What_is_Opus_Custom.3F
		and https://www.opus-codec.org/docs/opusfile_api-0.7/structOpusHead.html
	*/
	_rate = 48000;

	// Read in initial data
	if (!_fillBuffer())
		return;

	ogg_int64_t _pcmLength = op_pcm_total(_file, -1);
	if (_pcmLength < 0) {
		warning("Could not determine length of Opus stream: %s", opusError(_pcmLength));
	}
	_length = Timestamp(_pcmLength / _rate * 1000, _rate);
}

OpusStream::~OpusStream() {
	op_free(_file);
}

int OpusStream::readBuffer(int16 *buffer, const int numSamples) {
	int samples = 0;
	while (samples < numSamples && _pos < _bufferEnd) {
		const int len = MIN(numSamples - samples, (int)(_bufferEnd - _pos));
		memcpy(buffer, _pos, len * 2);
		buffer += len;
		_pos += len;
		samples += len;
		if (_pos >= _bufferEnd) {
			if (!_fillBuffer())
				break;
		}
	}

	return samples;
}

bool OpusStream::seek(const Timestamp &where) {
	// TODO
	// is 'false' correct here?
	int result = op_pcm_seek(_file, convertTimeToStreamPos(where, getRate(), false).totalNumberOfFrames());
	if (result < 0) {
		warning("Error seeking in Opus stream: %s", opusError(result));
		_pos = _bufferEnd;
		return false;
	}

	return _fillBuffer();
}

bool OpusStream::_fillBuffer() {
	uint bufferSize = sizeof(_buffer)/sizeof(*_buffer);
	uint samplesRead = 0;

	while (samplesRead < bufferSize) {
		// op_read() always returns data as 16 bit in native endianess
		int result = op_read(_file, _buffer + samplesRead, bufferSize - samplesRead, NULL);
		if (result < 0) {
			warning("Error reading from Opus stream: %s", opusError(result));
			if (result != OP_HOLE) {
				_pos = _bufferEnd;
				return false;
			}
		}

		if (result == 0)
			break; // EOF, done reading

		// op_read() returns number of samples read _per channel_
		samplesRead += result * _channels;
	}

	_pos = _buffer;
	_bufferEnd = _pos + samplesRead;

	return true;
}

#pragma mark -
#pragma mark --- Ogg Opus factory functions ---
#pragma mark -

SeekableAudioStream *makeOpusStream(
	Common::SeekableReadStream *stream,
	DisposeAfterUse::Flag disposeAfterUse) {
	SeekableAudioStream *s = new OpusStream(stream, disposeAfterUse);
	if (s && s->endOfData()) {
		delete s;
		return 0;
	} else {
		return s;
	}
}

} // End of namespace Audio

#endif // #ifdef USE_OPUS
