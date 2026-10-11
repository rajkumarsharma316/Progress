package com.example.streakbar

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.graphics.Color
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.os.IBinder
import android.provider.Settings
import android.text.TextUtils
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

class OverlayService : Service() {
    private lateinit var repository: StreakRepository
    private lateinit var windowManager: WindowManager
    private var overlayRoot: LinearLayout? = null
    private var isExpanded = false

    // Views
    private lateinit var streakText: TextView
    private lateinit var miniBoxesLayout: LinearLayout
    private lateinit var taskCountBadge: TextView
    private lateinit var expandButton: TextView
    private lateinit var expandedDrawer: LinearLayout
    private lateinit var dayStatusText: TextView
    private lateinit var toggleDayBtn: TextView
    private lateinit var taskInput: EditText
    private lateinit var tasksContainer: LinearLayout
    private lateinit var quoteText: TextView

    override fun onCreate() {
        super.onCreate()
        repository = StreakRepository(this)
        windowManager = getSystemService(WINDOW_SERVICE) as WindowManager
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_HIDE) {
            repository.setOverlayEnabled(false)
            stopSelf()
            return START_NOT_STICKY
        }

        if (intent?.action == ACTION_REFRESH) {
            if (overlayRoot != null) {
                refreshOverlay()
            }
            return START_STICKY
        }

        if (!Settings.canDrawOverlays(this)) {
            repository.setOverlayEnabled(false)
            stopSelf()
            return START_NOT_STICKY
        }

        repository.setOverlayEnabled(true)
        startAsForegroundService()
        if (overlayRoot == null) {
            addOverlay()
        } else {
            refreshOverlay()
        }
        return START_STICKY
    }

    override fun onDestroy() {
        overlayRoot?.let { windowManager.removeView(it) }
        overlayRoot = null
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    private fun addOverlay() {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundResource(R.drawable.overlay_bar)
            setPadding(dp(14), dp(6), dp(14), dp(8))
        }

        // --- 1. Top Header Bar (Always Visible: ~38dp) ---
        val header = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }

        // Fire icon 🔥
        val fireIcon = TextView(this).apply {
            text = "🔥"
            textSize = 13f
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, 0, dp(6), 0)
        }
        header.addView(fireIcon)

        // Streak Day Text (DAY X) - Clickable to toggle completed
        streakText = TextView(this).apply {
            textSize = 12f
            setTextColor(Color.rgb(248, 250, 252))
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, 0, dp(10), 0)
            setOnClickListener {
                repository.toggleToday()
                refreshOverlay()
            }
        }
        header.addView(streakText)

        // Mini Boxes Container (Middle)
        val middleLayout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL or Gravity.CENTER_HORIZONTAL
        }
        miniBoxesLayout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        taskCountBadge = TextView(this).apply {
            textSize = 11f
            setTextColor(Color.rgb(113, 113, 122))
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(4), 0, 0, 0)
        }
        middleLayout.addView(miniBoxesLayout)
        middleLayout.addView(taskCountBadge)
        header.addView(middleLayout, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))

        // Expand Chevron Button (▼ / ▲)
        expandButton = TextView(this).apply {
            text = "▼"
            textSize = 10f
            setTextColor(Color.rgb(161, 161, 170))
            gravity = Gravity.CENTER
            setPadding(dp(8), dp(4), dp(8), dp(4))
            setOnClickListener {
                toggleExpansion()
            }
        }
        header.addView(expandButton)

        // Close Button (×)
        val closeBtn = TextView(this).apply {
            text = "×"
            textSize = 16f
            setTextColor(Color.rgb(161, 161, 170))
            gravity = Gravity.CENTER
            setPadding(dp(6), 0, 0, 0)
            setOnClickListener {
                repository.setOverlayEnabled(false)
                stopSelf()
            }
        }
        header.addView(closeBtn)

        root.addView(header, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(26)))

        // --- 2. Expanded Drawer Section ---
        expandedDrawer = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            visibility = View.GONE
            setPadding(0, dp(8), 0, 0)
        }

        // Divider Line
        val divider = View(this).apply {
            setBackgroundColor(Color.rgb(54, 16, 22))
        }
        expandedDrawer.addView(divider, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(1)).apply {
            bottomMargin = dp(8)
        })

        // Day Status Card (Today: Completed / Not Completed + Action Button)
        val statusCard = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(10), dp(6), dp(10), dp(6))
            background = GradientDrawable().apply {
                setColor(Color.rgb(21, 9, 12))
                setStroke(dp(1), Color.rgb(74, 21, 28))
                cornerRadius = dp(6).toFloat()
            }
        }

        dayStatusText = TextView(this).apply {
            textSize = 11f
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            setTextColor(Color.rgb(239, 68, 68))
        }
        statusCard.addView(dayStatusText, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))

        toggleDayBtn = TextView(this).apply {
            textSize = 10f
            setTextColor(Color.WHITE)
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            setPadding(dp(8), dp(4), dp(8), dp(4))
            background = GradientDrawable().apply {
                setColor(Color.rgb(239, 68, 68))
                cornerRadius = dp(6).toFloat()
            }
            setOnClickListener {
                repository.toggleToday()
                refreshOverlay()
            }
        }
        statusCard.addView(toggleDayBtn)
        expandedDrawer.addView(statusCard, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply {
            bottomMargin = dp(8)
        })

        // Add Task Row (EditText + Add Button)
        val addRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }

        taskInput = EditText(this).apply {
            hint = "Type task and press Enter..."
            setHintTextColor(Color.rgb(113, 113, 122))
            setTextColor(Color.WHITE)
            textSize = 11f
            imeOptions = EditorInfo.IME_ACTION_DONE
            setSingleLine()
            setPadding(dp(8), dp(4), dp(8), dp(4))
            background = GradientDrawable().apply {
                setColor(Color.rgb(20, 9, 11))
                setStroke(dp(1), Color.rgb(74, 21, 28))
                cornerRadius = dp(6).toFloat()
            }
            setOnEditorActionListener { _, actionId, _ ->
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    submitTask()
                    true
                } else false
            }
        }
        addRow.addView(taskInput, LinearLayout.LayoutParams(0, dp(30), 1f).apply {
            rightMargin = dp(6)
        })

        val addBtn = TextView(this).apply {
            text = "+ Add"
            textSize = 11f
            setTextColor(Color.WHITE)
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            gravity = Gravity.CENTER
            setPadding(dp(10), 0, dp(10), 0)
            background = GradientDrawable().apply {
                setColor(Color.rgb(239, 68, 68))
                cornerRadius = dp(6).toFloat()
            }
            setOnClickListener {
                submitTask()
            }
        }
        addRow.addView(addBtn, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(30)))
        expandedDrawer.addView(addRow, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply {
            bottomMargin = dp(8)
        })

        // Task Items ScrollView
        val scroll = ScrollView(this)
        tasksContainer = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
        }
        scroll.addView(tasksContainer, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        expandedDrawer.addView(scroll, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(150)))

        // Daily Motivational Quote at Bottom
        val bottomDivider = View(this).apply {
            setBackgroundColor(Color.rgb(37, 11, 16))
        }
        expandedDrawer.addView(bottomDivider, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(1)).apply {
            topMargin = dp(6)
            bottomMargin = dp(6)
        })

        quoteText = TextView(this).apply {
            textSize = 10f
            setTextColor(Color.rgb(113, 113, 122))
            setTypeface(typeface, android.graphics.Typeface.ITALIC)
            gravity = Gravity.CENTER
            maxLines = 1
            ellipsize = TextUtils.TruncateAt.END
        }
        expandedDrawer.addView(quoteText, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))

        root.addView(expandedDrawer)

        // Overlay Window Parameters
        val screenWidth = resources.displayMetrics.widthPixels
        val width = (screenWidth * 0.92f).toInt().coerceAtMost(dp(380))

        val params = WindowManager.LayoutParams(
            width,
            ViewGroup.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT,
        ).apply {
            gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
            y = statusBarHeight() + dp(4)
        }

        overlayRoot = root
        windowManager.addView(root, params)
        refreshOverlay()
    }

    private fun submitTask() {
        val text = taskInput.text.toString()
        if (text.isNotBlank()) {
            repository.addTask(text)
            taskInput.text.clear()
            refreshOverlay()
        }
    }

    private fun toggleExpansion() {
        isExpanded = !isExpanded
        expandedDrawer.visibility = if (isExpanded) View.VISIBLE else View.GONE
        expandButton.text = if (isExpanded) "▲" else "▼"

        // Update WindowManager flags: allow focus for keyboard when expanded
        val params = overlayRoot?.layoutParams as? WindowManager.LayoutParams ?: return
        if (isExpanded) {
            params.flags = params.flags and WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE.inv()
        } else {
            params.flags = params.flags or WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
        }
        windowManager.updateViewLayout(overlayRoot, params)
    }

    private fun refreshOverlay() {
        val snapshot = repository.snapshot()

        // 1. Header Streak
        streakText.text = if (snapshot.completedToday) "DAY ${snapshot.currentStreak} ✓" else "DAY ${snapshot.currentStreak}"
        streakText.setTextColor(if (snapshot.completedToday) Color.rgb(239, 68, 68) else Color.rgb(248, 250, 252))

        // 2. Mini Indicator Boxes
        miniBoxesLayout.removeAllViews()
        if (snapshot.totalTasks == 0) {
            val emptyNotice = TextView(this).apply {
                text = "+ Add task"
                textSize = 10f
                setTextColor(Color.rgb(153, 27, 27))
                setOnClickListener { if (!isExpanded) toggleExpansion() }
            }
            miniBoxesLayout.addView(emptyNotice)
            taskCountBadge.text = ""
        } else {
            val maxBoxes = snapshot.tasks.size.coerceAtMost(5)
            for (i in 0 until maxBoxes) {
                val task = snapshot.tasks[i]
                val box = View(this).apply {
                    background = GradientDrawable().apply {
                        cornerRadius = dp(2).toFloat()
                        if (task.isCompleted) {
                            setColor(Color.rgb(239, 68, 68))
                        } else {
                            setColor(Color.rgb(26, 10, 13))
                            setStroke(dp(1), Color.rgb(85, 25, 32))
                        }
                    }
                }
                val boxParams = LinearLayout.LayoutParams(dp(9), dp(9)).apply {
                    rightMargin = dp(3)
                }
                miniBoxesLayout.addView(box, boxParams)
            }
            taskCountBadge.text = "${snapshot.completedTasks}/${snapshot.totalTasks}" + (if (snapshot.completedTasks == snapshot.totalTasks && snapshot.totalTasks > 0) " ✓" else "")
            taskCountBadge.setTextColor(
                if (snapshot.completedTasks == snapshot.totalTasks && snapshot.totalTasks > 0) Color.rgb(239, 68, 68) else Color.rgb(113, 113, 122)
            )
        }

        // 3. Status Card
        if (snapshot.completedToday) {
            dayStatusText.text = "Today: Completed ✓"
            dayStatusText.setTextColor(Color.rgb(239, 68, 68))
            toggleDayBtn.text = "↩ Undo"
            (toggleDayBtn.background as? GradientDrawable)?.setColor(Color.rgb(43, 14, 19))
        } else {
            dayStatusText.text = "Today: Not Completed"
            dayStatusText.setTextColor(Color.rgb(153, 27, 27))
            toggleDayBtn.text = "✓ Mark Done"
            (toggleDayBtn.background as? GradientDrawable)?.setColor(Color.rgb(239, 68, 68))
        }

        // 4. Task Items in Drawer
        tasksContainer.removeAllViews()
        if (snapshot.tasks.isEmpty()) {
            val emptyHint = TextView(this).apply {
                text = "No tasks yet today. Add one above!"
                textSize = 11f
                setTextColor(Color.rgb(113, 113, 122))
                setTypeface(typeface, android.graphics.Typeface.ITALIC)
                gravity = Gravity.CENTER
                setPadding(0, dp(16), 0, 0)
            }
            tasksContainer.addView(emptyHint)
        } else {
            for (task in snapshot.tasks) {
                val row = LinearLayout(this).apply {
                    orientation = LinearLayout.HORIZONTAL
                    gravity = Gravity.CENTER_VERTICAL
                    setPadding(0, dp(3), 0, dp(3))
                }

                // Checkbox
                val check = TextView(this).apply {
                    text = if (task.isCompleted) "✓" else ""
                    textSize = 10f
                    setTextColor(Color.WHITE)
                    gravity = Gravity.CENTER
                    background = GradientDrawable().apply {
                        cornerRadius = dp(3).toFloat()
                        if (task.isCompleted) {
                            setColor(Color.rgb(239, 68, 68))
                        } else {
                            setColor(Color.rgb(26, 10, 13))
                            setStroke(dp(1), Color.rgb(85, 25, 32))
                        }
                    }
                    setOnClickListener {
                        repository.toggleTask(task.id)
                        refreshOverlay()
                    }
                }
                row.addView(check, LinearLayout.LayoutParams(dp(16), dp(16)).apply {
                    rightMargin = dp(8)
                })

                // Title
                val title = TextView(this).apply {
                    text = task.title
                    textSize = 11f
                    if (task.isCompleted) {
                        setTextColor(Color.rgb(113, 113, 122))
                        paintFlags = paintFlags or Paint.STRIKE_THRU_TEXT_FLAG
                    } else {
                        setTextColor(Color.rgb(244, 244, 245))
                        paintFlags = paintFlags and Paint.STRIKE_THRU_TEXT_FLAG.inv()
                    }
                }
                row.addView(title, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))

                // Delete Button ✕
                val del = TextView(this).apply {
                    text = "✕"
                    textSize = 11f
                    setTextColor(Color.rgb(153, 27, 27))
                    setPadding(dp(8), dp(2), dp(4), dp(2))
                    setOnClickListener {
                        repository.deleteTask(task.id)
                        refreshOverlay()
                    }
                }
                row.addView(del)

                tasksContainer.addView(row)
            }
        }

        // 5. Quote
        quoteText.text = "“${snapshot.quote}”"
    }

    private fun startAsForegroundService() {
        val notification = notification()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE,
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun notification(): Notification {
        val openApp = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        val stopBar = PendingIntent.getService(
            this,
            1,
            Intent(this, OverlayService::class.java).setAction(ACTION_HIDE),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )

        return Notification.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.ic_popup_reminder)
            .setContentTitle("StreakBar is active")
            .setContentText("Your daily streak bar is pinned at the top.")
            .setContentIntent(openApp)
            .addAction(Notification.Action.Builder(null, "Stop bar", stopBar).build())
            .setOngoing(true)
            .build()
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "StreakBar overlay",
                NotificationManager.IMPORTANCE_LOW,
            ).apply {
                description = "Keeps the StreakBar overlay active."
            }
            getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
        }
    }

    private fun statusBarHeight(): Int {
        val id = resources.getIdentifier("status_bar_height", "dimen", "android")
        return if (id > 0) resources.getDimensionPixelSize(id) else 0
    }

    companion object {
        const val ACTION_SHOW = "com.example.streakbar.SHOW"
        const val ACTION_HIDE = "com.example.streakbar.HIDE"
        const val ACTION_REFRESH = "com.example.streakbar.REFRESH"
        private const val CHANNEL_ID = "streakbar_overlay"
        private const val NOTIFICATION_ID = 42
    }
}
