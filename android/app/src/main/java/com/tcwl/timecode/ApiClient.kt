package com.tcwl.timecode

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.FormBody
import okhttp3.OkHttpClient
import okhttp3.Request
import java.util.concurrent.TimeUnit

class ApiClient(private val host: String = "192.168.4.1") {

    private val client = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(5, TimeUnit.SECONDS)
        .writeTimeout(5, TimeUnit.SECONDS)
        .build()

    private val baseUrl: String get() = "http://$host"

    suspend fun setFps(fps: Int): Result<String> = post("/api/config", "fps", fps.toString())
    suspend fun setDropFrame(df: Boolean): Result<String> = post("/api/config", "df", if (df) "1" else "0")

    suspend fun jamTimecode(dd: Int, hh: Int, mm: Int, ss: Int, ff: Int): Result<String> {
        return withContext(Dispatchers.IO) {
            try {
                val body = FormBody.Builder()
                    .add("dd", dd.toString())
                    .add("hh", hh.toString())
                    .add("mm", mm.toString())
                    .add("ss", ss.toString())
                    .add("ff", ff.toString())
                    .build()
                val request = Request.Builder().url("$baseUrl/api/jam").post(body).build()
                val response = client.newCall(request).execute()
                Result.success(response.body?.string() ?: "ok")
            } catch (e: Exception) {
                Result.failure(e)
            }
        }
    }

    suspend fun setBrightness(value: Int): Result<String> = post("/api/brightness", "val", value.coerceIn(0, 15).toString())

    suspend fun setMatrixEnabled(en: Boolean): Result<String> = post("/api/matrix", "en", if (en) "1" else "0")

    suspend fun setOledEnabled(en: Boolean): Result<String> = post("/api/oled", "en", if (en) "1" else "0")

    suspend fun setLtcEnabled(en: Boolean): Result<String> = post("/api/ltc", "en", if (en) "1" else "0")

    suspend fun setBleName(name: String): Result<String> = withContext(Dispatchers.IO) {
        try {
            val body = FormBody.Builder()
                .add("action", "setname")
                .add("name", name)
                .build()
            val request = Request.Builder().url("$baseUrl/api/ble").post(body).build()
            val response = client.newCall(request).execute()
            Result.success(response.body?.string() ?: "ok")
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    suspend fun restart(): Result<String> = withContext(Dispatchers.IO) {
        try {
            val body = FormBody.Builder().build()
            val request = Request.Builder().url("$baseUrl/api/restart").post(body).build()
            val response = client.newCall(request).execute()
            Result.success("restarting")
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    private suspend fun post(path: String, key: String, value: String): Result<String> {
        return withContext(Dispatchers.IO) {
            try {
                val body = FormBody.Builder().add(key, value).build()
                val request = Request.Builder().url("$baseUrl$path").post(body).build()
                val response = client.newCall(request).execute()
                Result.success(response.body?.string() ?: "ok")
            } catch (e: Exception) {
                Result.failure(e)
            }
        }
    }
}
