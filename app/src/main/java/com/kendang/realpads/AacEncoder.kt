package com.kendang.realpads

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.MediaMuxer
import java.io.File
import java.nio.ByteBuffer

// Ubah hasil rekaman (PCM stereo interleaved dari AudioEngine.nativeStopRecording())
// jadi file M4A/AAC beneran, pake MediaCodec bawaan Android (bukan library native
// pihak ketiga kayak TAndroidLame sebelumnya). Alasan ganti dari MP3 ke AAC:
// libandroidlame.so bawaan TAndroidLame dikompilasi pake NDK lama yang gak support
// 16 KB page size, dan library-nya sendiri udah gak di-maintain sejak lama jadi gak
// bisa di-upgrade -> Play Console nolak/ngasih warning upload. MediaCodec itu API
// sistem Android sendiri (bukan .so yang ikut ke-bundle di APK/AAB), jadi masalah
// 16 KB ini otomatis gak akan pernah muncul lagi.
//
// Selalu dipanggil dari background thread (Dispatchers.IO) sama kayak encoder lama,
// karena walau cepat, ini tetap kerja CPU yang gak boleh nge-block UI thread buat
// rekaman yang agak panjang.
object AacEncoder {
    private const val SAMPLE_RATE = 48000
    private const val CHANNELS = 2
    private const val BITRATE_BPS = 192_000
    private const val MIME = MediaFormat.MIMETYPE_AUDIO_AAC
    private const val TIMEOUT_US = 10_000L

    // outputFile: perlu File biasa (bukan langsung OutputStream dari SAF) karena
    // MediaMuxer butuh path/FileDescriptor buat nulis container MP4 (.m4a), gak bisa
    // nulis ke OutputStream sembarangan. Makanya dipanggil dengan file temp dulu di
    // MainActivity, baru hasilnya di-copy ke Uri SAF yang dipilih user.
    fun encodeToAac(pcm: ShortArray, outputFile: File) {
        if (pcm.isEmpty()) return

        val format = MediaFormat.createAudioFormat(MIME, SAMPLE_RATE, CHANNELS).apply {
            setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC)
            setInteger(MediaFormat.KEY_BIT_RATE, BITRATE_BPS)
        }

        val codec = MediaCodec.createEncoderByType(MIME)
        codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
        codec.start()

        val muxer = MediaMuxer(outputFile.absolutePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)
        var trackIndex = -1
        var muxerStarted = false
        val bufferInfo = MediaCodec.BufferInfo()

        // PCM 16-bit ShortArray -> ByteArray (little-endian), sesuai yang diharapkan
        // input encoder AAC.
        val pcmBytes = ByteArray(pcm.size * 2)
        for (i in pcm.indices) {
            val v = pcm[i].toInt()
            pcmBytes[i * 2] = (v and 0xFF).toByte()
            pcmBytes[i * 2 + 1] = ((v shr 8) and 0xFF).toByte()
        }

        var offset = 0
        var presentationTimeUs = 0L
        val bytesPerSampleFrame = 2 * CHANNELS // 16-bit x 2 channel

        try {
            var inputDone = false
            var outputDone = false

            while (!outputDone) {
                if (!inputDone) {
                    val inputIndex = codec.dequeueInputBuffer(TIMEOUT_US)
                    if (inputIndex >= 0) {
                        val inputBuffer: ByteBuffer = codec.getInputBuffer(inputIndex)!!
                        inputBuffer.clear()
                        val remaining = pcmBytes.size - offset
                        if (remaining <= 0) {
                            codec.queueInputBuffer(inputIndex, 0, 0, presentationTimeUs, MediaCodec.BUFFER_FLAG_END_OF_STREAM)
                            inputDone = true
                        } else {
                            val chunkSize = minOf(inputBuffer.capacity(), remaining)
                            inputBuffer.put(pcmBytes, offset, chunkSize)
                            codec.queueInputBuffer(inputIndex, 0, chunkSize, presentationTimeUs, 0)
                            offset += chunkSize
                            val framesThisChunk = chunkSize / bytesPerSampleFrame
                            presentationTimeUs += (framesThisChunk * 1_000_000L) / SAMPLE_RATE
                        }
                    }
                }

                var outputIndex = codec.dequeueOutputBuffer(bufferInfo, TIMEOUT_US)
                while (outputIndex >= 0) {
                    if (bufferInfo.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG != 0) {
                        bufferInfo.size = 0 // config buffer gak ditulis langsung ke muxer
                    }
                    if (bufferInfo.size > 0 && muxerStarted) {
                        val encodedBuffer: ByteBuffer = codec.getOutputBuffer(outputIndex)!!
                        encodedBuffer.position(bufferInfo.offset)
                        encodedBuffer.limit(bufferInfo.offset + bufferInfo.size)
                        muxer.writeSampleData(trackIndex, encodedBuffer, bufferInfo)
                    }
                    val isEos = bufferInfo.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0
                    codec.releaseOutputBuffer(outputIndex, false)
                    if (isEos) {
                        outputDone = true
                        break
                    }
                    outputIndex = codec.dequeueOutputBuffer(bufferInfo, TIMEOUT_US)
                }
                if (outputIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                    trackIndex = muxer.addTrack(codec.outputFormat)
                    muxer.start()
                    muxerStarted = true
                }
            }
        } finally {
            try { codec.stop() } catch (_: Throwable) {}
            codec.release()
            try {
                if (muxerStarted) muxer.stop()
            } catch (_: Throwable) {}
            muxer.release()
        }
    }
}
