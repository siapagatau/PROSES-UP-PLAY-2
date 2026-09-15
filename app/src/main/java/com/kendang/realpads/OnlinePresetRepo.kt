package com.kendang.realpads

import org.json.JSONArray
import java.io.ByteArrayOutputStream
import java.net.HttpURLConnection
import java.net.URL

// Sumber preset online: repo GitHub publik yang isinya file .zip preset kendang
// (format .zip-nya sama persis kayak hasil tombol EXPORT di app ini: entry
// pad1.wav, pad2.wav, dst - atau pad1.mp3/m4a/dll, karena PresetStorage.importPreset
// udah bisa handle keduanya). Sengaja TIDAK butuh index.html/file manifest apapun
// di sisi repo - GitHub sendiri sudah nyediain REST API publik yang otomatis
// nge-list SEMUA file di sebuah folder repo dalam format JSON, jadi begitu ada
// yang upload .zip baru ke repo, app ini langsung "lihat" tanpa perlu update
// apa-apa lagi di sisi repo maupun di sisi app.
object OnlinePresetRepo {

    // Ganti 2 baris ini kalau suatu saat pindah owner/nama repo atau mau nunjuk
    // ke folder tertentu di dalam repo (bukan root).
    private const val REPO_OWNER = "siapagatau"
    private const val REPO_NAME = "preset-kendang-dtx-android"
    private const val REPO_PATH = "" // kosong = root repo

    private const val CONNECT_TIMEOUT_MS = 15_000
    private const val READ_TIMEOUT_MS = 30_000

    data class OnlinePreset(
        val name: String,        // nama file apa adanya, mis. "Kendang Sunda Kasar.zip"
        val downloadUrl: String, // URL langsung ke raw file, siap di-GET
        val sizeBytes: Long
    )

    // Ambil daftar semua file .zip yang ada di repo lewat GitHub Contents API:
    // https://api.github.com/repos/{owner}/{repo}/contents/{path}
    // Endpoint ini publik (gak butuh token) buat repo publik, dan hasilnya SELALU
    // up-to-date sesuai isi repo saat ini - gak ada cache manual di sisi kita.
    // Lempar Exception kalau gagal (network mati, repo gak ketemu, dll) - biar
    // pemanggil (UI) yang nentuin pesan errornya sendiri lewat try/catch.
    fun listPresets(): List<OnlinePreset> {
        val path = if (REPO_PATH.isBlank()) "" else "/$REPO_PATH"
        val apiUrl = "https://api.github.com/repos/$REPO_OWNER/$REPO_NAME/contents$path"
        val json = httpGetText(apiUrl)
        val arr = JSONArray(json)
        val result = mutableListOf<OnlinePreset>()
        for (i in 0 until arr.length()) {
            val obj = arr.getJSONObject(i)
            val type = obj.optString("type")
            val name = obj.optString("name")
            if (type == "file" && name.lowercase().endsWith(".zip")) {
                val downloadUrl = obj.optString("download_url")
                val size = obj.optLong("size", 0L)
                if (downloadUrl.isNotBlank()) {
                    result.add(OnlinePreset(name, downloadUrl, size))
                }
            }
        }
        // Urutkan A-Z biar daftarnya rapi & konsisten tiap dibuka, gak ngikutin
        // urutan upload commit di GitHub yang random dari sudut pandang user.
        return result.sortedBy { it.name.lowercase() }
    }

    // Download isi file .zip preset yang dipilih, langsung sebagai ByteArray -
    // dipanggil dari IO thread lalu byte-nya dioper ke
    // PresetStorage.importPreset(..., ByteArrayInputStream(bytes)) persis kayak
    // alur import file lokal biasa, jadi gak ada logic duplikat sama sekali.
    fun downloadZip(downloadUrl: String): ByteArray = httpGetBytes(downloadUrl)

    private fun httpGetText(urlStr: String): String = String(httpGetBytes(urlStr, acceptJson = true), Charsets.UTF_8)

    private fun httpGetBytes(urlStr: String, acceptJson: Boolean = false): ByteArray {
        val conn = URL(urlStr).openConnection() as HttpURLConnection
        try {
            conn.connectTimeout = CONNECT_TIMEOUT_MS
            conn.readTimeout = READ_TIMEOUT_MS
            conn.requestMethod = "GET"
            // User-Agent WAJIB diisi buat GitHub API - request tanpa User-Agent
            // ditolak (403) oleh server GitHub.
            conn.setRequestProperty("User-Agent", "DTX-Android-App")
            if (acceptJson) {
                conn.setRequestProperty("Accept", "application/vnd.github+json")
            }
            val code = conn.responseCode
            if (code !in 200..299) {
                val errBody = try {
                    conn.errorStream?.readBytes()?.let { String(it, Charsets.UTF_8) }
                } catch (e: Exception) { null }
                throw Exception("HTTP $code${if (errBody != null) ": $errBody" else ""}")
            }
            val out = ByteArrayOutputStream()
            conn.inputStream.use { it.copyTo(out) }
            return out.toByteArray()
        } finally {
            conn.disconnect()
        }
    }
}
