package com.example.streakbar

import android.Manifest
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

class MainActivity : Activity() {
    private lateinit var repository: StreakRepository
    private lateinit var streakDayText: TextView
    private lateinit var dailyBadgeText: TextView
    private lateinit var taskStatsText: TextView
    private lateinit var quoteText: TextView
    private lateinit var statusText: TextView
    private lateinit var syncStatusText: TextView
    private lateinit var toggleOverlayBtn: Button
    private lateinit var hideOverlayBtn: Button
    private lateinit var toggleDayBtn: Button
    private var awaitingOverlayPermission = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        repository = StreakRepository(this)
        requestNotificationPermissionIfNeeded()
        setContentView(createContent())
    }

    override fun onResume() {
        super.onResume()
        if (awaitingOverlayPermission && Settings.canDrawOverlays(this)) {
            awaitingOverlayPermission = false
            showOverlay()
        }
        refreshContent()
    }

    private fun createContent(): View {
        val root = ScrollView(this).apply {
            setBackgroundColor(Color.parseColor("#0D080A"))
            isFillViewport = true
        }

        val container = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(24), dp(44), dp(24), dp(36))

            // Branding Header
            addView(LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                addView(TextView(context).apply {
                    text = "🔥"
                    textSize = 20f
                    setPadding(0, 0, dp(8), 0)
                })
                addView(TextView(context).apply {
                    text = "STREAKBAR"
                    setTextColor(Color.parseColor("#EF4444"))
                    textSize = 15f
                    typeface = Typeface.DEFAULT_BOLD
                    letterSpacing = 0.2f
                })
            })

            // Hero Card
            val heroCard = LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(dp(20), dp(20), dp(20), dp(20))
                background = GradientDrawable().apply {
                    setColor(Color.parseColor("#15090C"))
                    cornerRadius = dp(14).toFloat()
                    setStroke(dp(1), Color.parseColor("#4A1218"))
                }

                streakDayText = TextView(context).apply {
                    setTextColor(Color.parseColor("#F8FAFC"))
                    textSize = 34f
                    typeface = Typeface.DEFAULT_BOLD
                }
                addView(streakDayText)

                dailyBadgeText = TextView(context).apply {
                    textSize = 13f
                    typeface = Typeface.DEFAULT_BOLD
                    setPadding(0, dp(4), 0, dp(12))
                }
                addView(dailyBadgeText)

                taskStatsText = TextView(context).apply {
                    setTextColor(Color.parseColor("#A1A1AA"))
                    textSize = 14f
                }
                addView(taskStatsText)

                toggleDayBtn = Button(context).apply {
                    typeface = Typeface.DEFAULT_BOLD
                    textSize = 13f
                    setOnClickListener {
                        repository.toggleToday()
                        refreshContent()
                        if (repository.overlayEnabled()) {
                            showOverlay()
                        }
                    }
                }
                val btnLp = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    dp(42)
                ).apply { topMargin = dp(14) }
                addView(toggleDayBtn, btnLp)
            }

            val heroLp = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = dp(20) }
            addView(heroCard, heroLp)

            // Quote Card
            val quoteCard = LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(dp(16), dp(16), dp(16), dp(16))
                background = GradientDrawable().apply {
                    setColor(Color.parseColor("#12070A"))
                    cornerRadius = dp(10).toFloat()
                    setStroke(dp(1), Color.parseColor("#380C12"))
                }

                quoteText = TextView(context).apply {
                    setTextColor(Color.parseColor("#FDA4AF"))
                    textSize = 13f
                    typeface = Typeface.create(Typeface.DEFAULT, Typeface.ITALIC)
                    gravity = Gravity.CENTER
                }
                addView(quoteText)
            }
            val quoteLp = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = dp(14) }
            addView(quoteCard, quoteLp)

            // Wi-Fi Sync Section Header & Card
            addView(TextView(context).apply {
                text = "WI-FI SYNC WITH PC"
                setTextColor(Color.parseColor("#71717A"))
                textSize = 11f
                typeface = Typeface.DEFAULT_BOLD
                letterSpacing = 0.15f
                setPadding(0, dp(24), 0, dp(8))
            })

            val syncCard = LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(dp(16), dp(16), dp(16), dp(16))
                background = GradientDrawable().apply {
                    setColor(Color.parseColor("#15090C"))
                    cornerRadius = dp(12).toFloat()
                    setStroke(dp(1), Color.parseColor("#4A1218"))
                }

                addView(TextView(context).apply {
                    text = "Sync tasks and streak in real-time over your local Wi-Fi with StreakBar on Windows."
                    setTextColor(Color.parseColor("#A1A1AA"))
                    textSize = 12f
                    setPadding(0, 0, 0, dp(12))
                })

                val ipInput = EditText(context).apply {
                    hint = "PC IP (e.g. 192.168.1.15)"
                    setHintTextColor(Color.parseColor("#52525B"))
                    setTextColor(Color.parseColor("#F8FAFC"))
                    textSize = 14f
                    setText(repository.getPcHost())
                    background = GradientDrawable().apply {
                        setColor(Color.parseColor("#1E0A0F"))
                        cornerRadius = dp(8).toFloat()
                        setStroke(dp(1), Color.parseColor("#4A1218"))
                    }
                    setPadding(dp(12), dp(10), dp(12), dp(10))
                }
                addView(ipInput)

                syncStatusText = TextView(context).apply {
                    text = if (repository.getPcHost().isNotBlank()) "🟢 Host configured: ${repository.getPcHost()}" else "⚪ Enter PC IP (from StreakBar on PC)"
                    setTextColor(Color.parseColor("#71717A"))
                    textSize = 12f
                    setPadding(0, dp(8), 0, dp(10))
                }
                addView(syncStatusText)

                val syncBtn = Button(context).apply {
                    text = "Link & Sync Now"
                    setTextColor(Color.WHITE)
                    textSize = 13f
                    typeface = Typeface.DEFAULT_BOLD
                    background = GradientDrawable().apply {
                        setColor(Color.parseColor("#EF4444"))
                        cornerRadius = dp(8).toFloat()
                    }
                    setOnClickListener {
                        val host = ipInput.text.toString().trim()
                        if (host.isNotBlank()) {
                            repository.setPcHost(host)
                            syncStatusText.text = "⏳ Connecting to PC..."
                            syncStatusText.setTextColor(Color.parseColor("#F59E0B"))
                            repository.pullFromPc { success ->
                                runOnUiThread {
                                    if (success) {
                                        syncStatusText.text = "🟢 Synced successfully with PC!"
                                        syncStatusText.setTextColor(Color.parseColor("#22C55E"))
                                        refreshContent()
                                        if (repository.overlayEnabled()) {
                                            val refreshIntent = Intent(this@MainActivity, OverlayService::class.java)
                                                .setAction(OverlayService.ACTION_REFRESH)
                                            startService(refreshIntent)
                                        }
                                    } else {
                                        syncStatusText.text = "🔴 Connection failed (ensure on same Wi-Fi)"
                                        syncStatusText.setTextColor(Color.parseColor("#EF4444"))
                                    }
                                }
                            }
                        }
                    }
                }
                addView(syncBtn, LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    dp(42)
                ))
            }
            addView(syncCard)

            // Controls Section Header
            addView(TextView(context).apply {
                text = "OVERLAY DISPLAY"
                setTextColor(Color.parseColor("#71717A"))
                textSize = 11f
                typeface = Typeface.DEFAULT_BOLD
                letterSpacing = 0.15f
                setPadding(0, dp(24), 0, dp(8))
            })

            // Primary Toggle Button
            toggleOverlayBtn = Button(context).apply {
                setTextColor(Color.WHITE)
                textSize = 14f
                typeface = Typeface.DEFAULT_BOLD
                background = GradientDrawable().apply {
                    setColor(Color.parseColor("#EF4444"))
                    cornerRadius = dp(10).toFloat()
                }
                setOnClickListener {
                    if (Settings.canDrawOverlays(this@MainActivity)) {
                        showOverlay()
                    } else {
                        requestOverlayPermission()
                    }
                }
            }
            addView(toggleOverlayBtn, LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                dp(48)
            ))

            // Hide Button
            hideOverlayBtn = Button(context).apply {
                text = "Hide Floating Notch"
                setTextColor(Color.parseColor("#EF4444"))
                textSize = 13f
                typeface = Typeface.DEFAULT_BOLD
                background = GradientDrawable().apply {
                    setColor(Color.parseColor("#2B0E13"))
                    cornerRadius = dp(10).toFloat()
                    setStroke(dp(1), Color.parseColor("#4A1218"))
                }
                setOnClickListener { hideOverlay() }
            }
            val hideLp = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                dp(44)
            ).apply { topMargin = dp(10) }
            addView(hideOverlayBtn, hideLp)

            // Status message
            statusText = TextView(context).apply {
                setTextColor(Color.parseColor("#A1A1AA"))
                textSize = 12f
                gravity = Gravity.CENTER_HORIZONTAL
                setPadding(0, dp(16), 0, 0)
            }
            addView(statusText)

            // Disclaimer / Note
            addView(TextView(context).apply {
                text = "Runs directly on your screen with zero tracking. Pinned to top notch with real-time PC sync."
                setTextColor(Color.parseColor("#52525B"))
                textSize = 11f
                gravity = Gravity.CENTER_HORIZONTAL
                setPadding(0, dp(20), 0, 0)
            })
        }

        root.addView(container)
        return root
    }

    private fun refreshContent() {
        if (!::streakDayText.isInitialized) return
        val snapshot = repository.snapshot()

        streakDayText.text = if (snapshot.currentStreak == 0) "Day 0" else "Day ${snapshot.currentStreak}"

        if (snapshot.completedToday) {
            dailyBadgeText.text = "✓ COMPLETED TODAY"
            dailyBadgeText.setTextColor(Color.parseColor("#EF4444"))
            toggleDayBtn.text = "Undo Today's Streak"
            toggleDayBtn.setTextColor(Color.parseColor("#A1A1AA"))
            toggleDayBtn.background = GradientDrawable().apply {
                setColor(Color.parseColor("#2B0E13"))
                cornerRadius = dp(8).toFloat()
                setStroke(dp(1), Color.parseColor("#4A1218"))
            }
        } else {
            dailyBadgeText.text = "PENDING DAILY TASKS"
            dailyBadgeText.setTextColor(Color.parseColor("#F59E0B"))
            toggleDayBtn.text = "Mark Today Done"
            toggleDayBtn.setTextColor(Color.WHITE)
            toggleDayBtn.background = GradientDrawable().apply {
                setColor(Color.parseColor("#EF4444"))
                cornerRadius = dp(8).toFloat()
            }
        }

        taskStatsText.text = if (snapshot.totalTasks == 0) {
            "No tasks added yet (add in floating drawer)"
        } else {
            "${snapshot.completedTasks} of ${snapshot.totalTasks} tasks finished today"
        }

        quoteText.text = "\"${snapshot.quote}\""

        val canOverlay = Settings.canDrawOverlays(this)
        toggleOverlayBtn.text = if (canOverlay) "Show Floating Notch" else "Grant Permission & Show Notch"
        statusText.text = when {
            !canOverlay -> "Permission needed: allow 'Display over other apps'"
            repository.overlayEnabled() -> "Floating notch is currently active on your screen"
            else -> "Ready to launch floating notch"
        }
    }

    private fun requestOverlayPermission() {
        awaitingOverlayPermission = true
        val intent = Intent(
            Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
            Uri.parse("package:$packageName")
        )
        startActivity(intent)
    }

    private fun showOverlay() {
        repository.setOverlayEnabled(true)
        val intent = Intent(this, OverlayService::class.java).setAction(OverlayService.ACTION_SHOW)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(intent)
        } else {
            startService(intent)
        }
        refreshContent()
    }

    private fun hideOverlay() {
        repository.setOverlayEnabled(false)
        stopService(Intent(this, OverlayService::class.java))
        refreshContent()
    }

    private fun requestNotificationPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
        ) {
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), NOTIFICATION_PERMISSION_REQUEST)
        }
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    private companion object {
        const val NOTIFICATION_PERMISSION_REQUEST = 100
    }
}
