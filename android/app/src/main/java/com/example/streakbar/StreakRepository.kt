package com.example.streakbar

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.HttpURLConnection
import java.net.URL
import java.time.LocalDate

data class TaskItem(
    val id: String,
    val title: String,
    val isCompleted: Boolean = false
)

data class StreakSnapshot(
    val currentStreak: Int,
    val completedToday: Boolean,
    val quote: String,
    val totalTasks: Int,
    val completedTasks: Int,
    val tasks: List<TaskItem>
)

class StreakRepository(context: Context) {
    private val preferences = context.getSharedPreferences("streak_bar", Context.MODE_PRIVATE)

    fun snapshot(): StreakSnapshot {
        checkRollover()
        val today = LocalDate.now()
        val completedToday = preferences.getString(LAST_COMPLETED, null) == today.toString()
        val tasks = loadTasks()
        val total = tasks.size
        val completed = tasks.count { it.isCompleted }

        return StreakSnapshot(
            currentStreak = preferences.getInt(CURRENT_STREAK, 0),
            completedToday = completedToday,
            quote = QUOTES[(today.dayOfYear - 1) % QUOTES.size],
            totalTasks = total,
            completedTasks = completed,
            tasks = tasks
        )
    }

    fun completeToday() {
        checkRollover()
        val today = LocalDate.now().toString()
        if (preferences.getString(LAST_COMPLETED, null) == today) return

        val currentStreak = preferences.getInt(CURRENT_STREAK, 1)
        val validStreak = if (currentStreak <= 0) 1 else currentStreak

        preferences.edit()
            .putInt(CURRENT_STREAK, validStreak)
            .putString(LAST_COMPLETED, today)
            .apply()
    }

    fun uncompleteToday() {
        checkRollover()
        val today = LocalDate.now().toString()
        if (preferences.getString(LAST_COMPLETED, null) != today) return

        preferences.edit()
            .putString(LAST_COMPLETED, "")
            .apply()
    }

    fun toggleToday() {
        val today = LocalDate.now().toString()
        if (preferences.getString(LAST_COMPLETED, null) == today) {
            uncompleteToday()
        } else {
            completeToday()
        }
    }

    fun addTask(title: String) {
        if (title.isBlank()) return
        checkRollover()
        val tasks = loadTasks().toMutableList()
        val id = System.currentTimeMillis().toString() + "_" + (tasks.size + 1)
        tasks.add(TaskItem(id, title.trim(), false))
        saveTasks(tasks)
    }

    fun toggleTask(id: String) {
        checkRollover()
        val tasks = loadTasks().map {
            if (it.id == id) it.copy(isCompleted = !it.isCompleted) else it
        }
        saveTasks(tasks)
        if (tasks.isNotEmpty() && tasks.all { it.isCompleted }) {
            completeToday()
        }
    }

    fun deleteTask(id: String) {
        checkRollover()
        val tasks = loadTasks().filter { it.id != id }
        saveTasks(tasks)
    }

    fun getPcHost(): String = preferences.getString(PC_HOST, "") ?: ""

    fun setPcHost(host: String) {
        val cleanHost = host.trim().removePrefix("http://").removePrefix("https://").substringBefore(":")
        preferences.edit().putString(PC_HOST, cleanHost).apply()
    }

    fun pushToPc() {
        val host = getPcHost()
        if (host.isBlank()) return

        Thread {
            try {
                val url = URL("http://$host:54321/api/sync")
                val conn = url.openConnection() as HttpURLConnection
                conn.requestMethod = "POST"
                conn.connectTimeout = 2000
                conn.readTimeout = 2000
                conn.doOutput = true
                conn.setRequestProperty("Content-Type", "application/json")

                val json = JSONObject()
                json.put("CurrentStreak", preferences.getInt(CURRENT_STREAK, 0))
                json.put("LastCompletedDate", preferences.getString(LAST_COMPLETED, "") ?: "")
                json.put("PreviousStreak", preferences.getInt(PREV_STREAK, 0))
                json.put("PreviousCompletedDate", preferences.getString(PREV_COMPLETED, "") ?: "")
                json.put("TasksDate", preferences.getString(TASKS_DATE, "") ?: "")

                val tasksArray = JSONArray()
                for (t in loadTasks()) {
                    val itemObj = JSONObject()
                    itemObj.put("Id", t.id)
                    itemObj.put("Title", t.title)
                    itemObj.put("IsCompleted", t.isCompleted)
                    tasksArray.put(itemObj)
                }
                json.put("Tasks", tasksArray)

                OutputStreamWriter(conn.outputStream).use { it.write(json.toString()) }
                conn.responseCode
                conn.disconnect()
            } catch (_: Exception) {}
        }.start()
    }

    fun pullFromPc(onComplete: ((Boolean) -> Unit)? = null) {
        val host = getPcHost()
        if (host.isBlank()) {
            onComplete?.invoke(false)
            return
        }

        Thread {
            try {
                val url = URL("http://$host:54321/api/sync")
                val conn = url.openConnection() as HttpURLConnection
                conn.requestMethod = "GET"
                conn.connectTimeout = 2500
                conn.readTimeout = 2500

                if (conn.responseCode == 200) {
                    val reader = BufferedReader(InputStreamReader(conn.inputStream))
                    val sb = StringBuilder()
                    var line: String?
                    while (reader.readLine().also { line = it } != null) {
                        sb.append(line)
                    }
                    reader.close()

                    val json = JSONObject(sb.toString())
                    val pcStreak = json.optInt("CurrentStreak", 0)
                    if (pcStreak > 0) {
                        preferences.edit()
                            .putInt(CURRENT_STREAK, pcStreak)
                            .apply()
                    }
                    conn.disconnect()
                    onComplete?.invoke(true)
                } else {
                    conn.disconnect()
                    onComplete?.invoke(false)
                }
            } catch (e: Exception) {
                onComplete?.invoke(false)
            }
        }.start()
    }

    private fun checkRollover() {
        val today = LocalDate.now()
        val todayStr = today.toString()
        val tasksDate = preferences.getString(TASKS_DATE, null)
        if (tasksDate != null && tasksDate != todayStr) {
            val lastCompStr = preferences.getString(LAST_COMPLETED, null)
            val yesterdayStr = today.minusDays(1).toString()
            val currentStreak = preferences.getInt(CURRENT_STREAK, 1)

            val newStreak = if (lastCompStr == yesterdayStr) {
                currentStreak + 1
            } else if (lastCompStr == todayStr) {
                currentStreak
            } else {
                1
            }

            preferences.edit()
                .putInt(CURRENT_STREAK, newStreak)
                .putString(TASKS_DATE, todayStr)
                .apply()

            val tasks = loadTasks().map { it.copy(isCompleted = false) }
            saveTasks(tasks)
        } else if (tasksDate == null) {
            preferences.edit().putString(TASKS_DATE, todayStr).apply()
        }
    }

    private fun loadTasks(): List<TaskItem> {
        val json = preferences.getString(TASKS_JSON, "[]") ?: "[]"
        return try {
            val array = JSONArray(json)
            val list = mutableListOf<TaskItem>()
            for (i in 0 until array.length()) {
                val obj = array.getJSONObject(i)
                list.add(
                    TaskItem(
                        id = obj.getString("id"),
                        title = obj.getString("title"),
                        isCompleted = obj.optBoolean("isCompleted", false)
                    )
                )
            }
            list
        } catch (e: Exception) {
            emptyList()
        }
    }

    private fun saveTasks(tasks: List<TaskItem>) {
        val array = JSONArray()
        for (t in tasks) {
            val obj = JSONObject()
            obj.put("id", t.id)
            obj.put("title", t.title)
            obj.put("isCompleted", t.isCompleted)
            array.put(obj)
        }
        preferences.edit().putString(TASKS_JSON, array.toString()).apply()
    }

    fun overlayEnabled(): Boolean = preferences.getBoolean(OVERLAY_ENABLED, false)

    fun setOverlayEnabled(enabled: Boolean) {
        preferences.edit().putBoolean(OVERLAY_ENABLED, enabled).apply()
    }

    companion object {
        const val CURRENT_STREAK = "current_streak"
        const val LAST_COMPLETED = "last_completed"
        const val PREV_STREAK = "prev_streak"
        const val PREV_COMPLETED = "prev_completed"
        const val TASKS_DATE = "tasks_date"
        const val TASKS_JSON = "tasks_json"
        const val OVERLAY_ENABLED = "overlay_enabled"
        const val PC_HOST = "pc_host"

        val QUOTES = listOf(
            "Small daily promises become a remarkable life.",
            "Start where you are. Use what you have. Do what you can.",
            "Consistency is a quiet kind of courage.",
            "A little progress today is still progress.",
            "Your future self is built by today’s choices.",
            "Keep the promise you made to yourself.",
        )
    }
}
