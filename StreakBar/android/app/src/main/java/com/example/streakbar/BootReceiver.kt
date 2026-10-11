package com.example.streakbar

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Build
import android.provider.Settings

class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != Intent.ACTION_BOOT_COMPLETED) return

        val repository = StreakRepository(context)
        if (!repository.overlayEnabled() || !Settings.canDrawOverlays(context)) return

        val serviceIntent = Intent(context, OverlayService::class.java).setAction(OverlayService.ACTION_SHOW)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            context.startForegroundService(serviceIntent)
        } else {
            context.startService(serviceIntent)
        }
    }
}
