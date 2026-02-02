package wtf.flutter.vr_player

import android.content.Context
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View
import android.widget.FrameLayout
import com.kaltura.playkit.*
import com.kaltura.playkit.player.vr.VRInteractionMode
import com.kaltura.playkit.player.vr.VRSettings
import com.kaltura.playkit.providers.ovp.OVPMediaAsset
import com.kaltura.playkitvr.VRController
import com.kaltura.playkitvr.VRUtil
import com.kaltura.tvplayer.KalturaBasicPlayer
import com.kaltura.tvplayer.OVPMediaOptions
import com.kaltura.tvplayer.PlayerInitOptions
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel

class VideoPlayerController(
    private val context: Context,
    private val listener: ViewCreatedListener,
    viewId: Int,
    private val binaryMessenger: BinaryMessenger
) : MethodChannel.MethodCallHandler {
    private var videoUrl: HashMap<*, *>? = null
    private var localPath: String? = null

    private var player: KalturaBasicPlayer? = null
    private var mediaEntry: PKMediaEntry? = null
    private var videoPlayerState: VideoPlayerState? = null
    private var isVRModeEnabled: Boolean = false

    private var playerEventStateChanged: EventChannel.EventSink? = null
    private var playerEventDurationChanged: EventChannel.EventSink? = null
    private var playerEventPosition: EventChannel.EventSink? = null
    private var playerEventEnded: EventChannel.EventSink? = null
    private var continuousDragAnimator: android.animation.ValueAnimator? = null
    private var currentDragX: Float = 0f
    private var currentDragY: Float = 0f
    private var lastDownTime: Long = 0L
    private var lastVelocityX: Float = 0f
    private var lastVelocityY: Float = 0f

    interface ViewCreatedListener {
        fun onViewCreated(view: View)
        fun changeViewSize(args: HashMap<*, *>)
    }

    init {
        MethodChannel(binaryMessenger, "vr_player_$viewId").apply {
            setMethodCallHandler(this@VideoPlayerController)
        }
        createEventChannel(
            "vr_player_events_${viewId}_state",
            { e -> playerEventStateChanged = e },
            { playerEventStateChanged = null })
        createEventChannel(
            "vr_player_events_${viewId}_duration",
            { e -> playerEventDurationChanged = e },
            { playerEventDurationChanged = null })
        createEventChannel(
            "vr_player_events_${viewId}_position",
            { e -> playerEventPosition = e },
            { playerEventPosition = null })
        createEventChannel(
            "vr_player_events_${viewId}_ended",
            { e -> playerEventEnded = e },
            { playerEventEnded = null })
    }

    private fun createEventChannel(
        name: String,
        onListen: (events: EventChannel.EventSink?) -> Unit,
        onCancel: () -> Unit
    ) {
        EventChannel(binaryMessenger, name).setStreamHandler(object : EventChannel.StreamHandler {
            override fun onListen(arguments: Any?, events: EventChannel.EventSink?) {
                onListen(events)
            }

            override fun onCancel(arguments: Any?) {
                onCancel()
            }
        })
    }

    companion object {
        private const val TAG = "VrController"
    }

    override fun onMethodCall(methodCall: MethodCall, result: MethodChannel.Result) {
        when (methodCall.method) {
            "loadVideo" -> {
                (methodCall.arguments as? HashMap<*, *>)?.let {
                    loadMedia(it, result)
                }
            }
            "seekTo" -> {
                (methodCall.arguments as? HashMap<*, *>)?.get("position")?.toString()
                    ?.toLongOrNull()?.let { position ->
                    player?.let {
                        if (position < it.duration) {
                            it.seekTo(position)
                            result.success(methodCall.arguments)
                        }
                    }
                }
            }
            "setVolume" -> {
                val volume: Float = methodCall.argument("volume")!!
                player?.setVolume(volume)
            }
            "play" -> {
                player?.play()
                result.success(true)
            }
            "pause" -> {
                player?.pause()
                result.success(true)
            }
            "isPlaying" -> {
                result.success(player?.isPlaying == true)
            }
            "dispose" -> {
                dispose()
                result.success(true)
            }
            "onSizeChanged" -> {
                if (mediaEntry?.isVRMediaType == true) {
                    reloadPlayer()
                }

                (methodCall.arguments as? HashMap<*, *>)?.let {
                    listener.changeViewSize(it)
                }
                result.success(true)
            }
            "toggleVRMode" -> {
                toggleVRMode()
            }
            "fullScreen" -> {
                if (mediaEntry?.isVRMediaType == true) {
                    dispose()
                }
                result.success(true)
            }
            "onPause" -> {
                if (mediaEntry?.isVRMediaType == true) {
                    dispose()
                } else {
                    player?.onApplicationPaused()
                }
                result.success(true)
            }
            "onResume" -> {
                if (mediaEntry?.isVRMediaType == true) {
                    reloadPlayer()
                } else {
                    player?.onApplicationResumed()
                }
                result.success(true)
            }
            "onOrientationChanged" -> {
                if (mediaEntry?.isVRMediaType == true) {
                    dispose()
                }
                result.success(true)
            }
            "simulateTouch" -> {
                val eventType = methodCall.argument<Int>("eventType")
                val x = methodCall.argument<Double>("x")?.toFloat()
                val y = methodCall.argument<Double>("y")?.toFloat()

                if (eventType != null && x != null && y != null) {
                    val duration = SystemClock.uptimeMillis()
                    val event = MotionEvent.obtain(
                        duration,
                        duration,
                        eventType,
                        x,
                        y,
                        0
                    )
                    player?.playerView?.dispatchTouchEvent(event)
                    event.recycle()
                    result.success(true)
                } else {
                    result.error("INVALID_ARGUMENTS", "Missing arguments", null)
                }
            }
            "simulateDrag" -> {
                val x1 = methodCall.argument<Double>("x1")?.toFloat()
                val y1 = methodCall.argument<Double>("y1")?.toFloat()
                val x2 = methodCall.argument<Double>("x2")?.toFloat()
                val y2 = methodCall.argument<Double>("y2")?.toFloat()
                val duration = methodCall.argument<Int>("duration")?.toLong() ?: 300L

                if (x1 != null && y1 != null && x2 != null && y2 != null) {
                    continuousDragAnimator?.cancel()
                    
                    val downTime = SystemClock.uptimeMillis()
                    val downEvent = MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, x1, y1, 0)
                    player?.playerView?.dispatchTouchEvent(downEvent)
                    downEvent.recycle()

                    continuousDragAnimator = android.animation.ValueAnimator.ofFloat(0f, 1f).apply {
                        this.duration = duration
                        this.interpolator = android.view.animation.LinearInterpolator()
                        
                        addUpdateListener { animation ->
                            val t = animation.animatedValue as Float
                            val x = x1 + (x2 - x1) * t
                            val y = y1 + (y2 - y1) * t
                            
                            val moveTime = SystemClock.uptimeMillis()
                            val moveEvent = MotionEvent.obtain(downTime, moveTime, MotionEvent.ACTION_MOVE, x, y, 0)
                            player?.playerView?.dispatchTouchEvent(moveEvent)
                            moveEvent.recycle()
                        }

                        addListener(object : android.animation.AnimatorListenerAdapter() {
                            override fun onAnimationEnd(animation: android.animation.Animator) {
                                val upTime = SystemClock.uptimeMillis()
                                val upEvent = MotionEvent.obtain(downTime, upTime, MotionEvent.ACTION_UP, x2, y2, 0)
                                player?.playerView?.dispatchTouchEvent(upEvent)
                                upEvent.recycle()
                            }
                        })
                        start()
                    }
                    result.success(true)
                } else {
                     result.error("INVALID_ARGUMENTS", "Missing arguments", null)
                }
            }
            "startContinuousDrag" -> {
                val velocityX = methodCall.argument<Double>("dx")?.toFloat()
                val velocityY = methodCall.argument<Double>("dy")?.toFloat()

                if (velocityX != null && velocityY != null) {
                    continuousDragAnimator?.cancel()
                    
                    player?.playerView?.let { view ->
                        currentDragX = view.width / 2f
                        currentDragY = view.height / 2f
                        lastDownTime = SystemClock.uptimeMillis()
                        lastVelocityX = velocityX
                        lastVelocityY = velocityY
                        
                        // Dispatch DOWN
                        val downEvent = MotionEvent.obtain(lastDownTime, lastDownTime, MotionEvent.ACTION_DOWN, currentDragX, currentDragY, 0)
                        view.dispatchTouchEvent(downEvent)
                        downEvent.recycle()

                        var lastFrameTime = lastDownTime
                        var lastDispatchTime = 0L

                        continuousDragAnimator = android.animation.ValueAnimator.ofFloat(0f, 1f).apply {
                            duration = 1000 
                            repeatCount = android.animation.ValueAnimator.INFINITE
                            interpolator = android.view.animation.LinearInterpolator()
                            
                            addUpdateListener {
                                val now = SystemClock.uptimeMillis()
                                if (now - lastDispatchTime < 33) {
                                    return@addUpdateListener
                                }
                                lastDispatchTime = now

                                val dt = (now - lastFrameTime) / 1000f
                                lastFrameTime = now
                                
                                currentDragX += velocityX * dt
                                currentDragY += velocityY * dt
                                
                                val moveEvent = MotionEvent.obtain(lastDownTime, now, MotionEvent.ACTION_MOVE, currentDragX, currentDragY, 0)
                                view.dispatchTouchEvent(moveEvent)
                                moveEvent.recycle()
                            }
                            start()
                        }
                    }
                    result.success(true)
                } else {
                    result.error("INVALID_ARGUMENTS", "Missing arguments", null)
                }
            }
            "stopContinuousDrag" -> {
                continuousDragAnimator?.cancel()
                continuousDragAnimator = null
                
                player?.playerView?.let { view ->
                     val now = SystemClock.uptimeMillis()
                     val duration = now - lastDownTime
                     
                     // "Tap" compensation: Ensure minimum movement if held briefly
                     // 150ms threshold
                     if (duration < 150) {
                         // Force move by 150ms worth of velocity (or remaining)
                         // We just add a delta equivalent to 0.1s ensures a visible jump
                         val simulateDt = 0.1f // 100ms equivalent jump
                         currentDragX += lastVelocityX * simulateDt
                         currentDragY += lastVelocityY * simulateDt
                         
                         val moveEvent = MotionEvent.obtain(lastDownTime, now, MotionEvent.ACTION_MOVE, currentDragX, currentDragY, 0)
                         view.dispatchTouchEvent(moveEvent)
                         moveEvent.recycle()
                     }

                     val upEvent = MotionEvent.obtain(lastDownTime, now, MotionEvent.ACTION_UP, currentDragX, currentDragY, 0)
                     view.dispatchTouchEvent(upEvent)
                     upEvent.recycle()
                }
                result.success(true)
            }
            "stopContinuousDrag" -> {
                continuousDragAnimator?.cancel()
                continuousDragAnimator = null
                
                player?.playerView?.let { view ->
                     val upTime = SystemClock.uptimeMillis()
                     val upEvent = MotionEvent.obtain(upTime, upTime, MotionEvent.ACTION_UP, currentDragX, currentDragY, 0)
                     view.dispatchTouchEvent(upEvent)
                     upEvent.recycle()
                }
                result.success(true)
            }
            else -> result.notImplemented()
        }
    }

    private fun reloadPlayer() {
        loadPlayer()
        this.videoUrl?.let {
            loadMedia(it, null)
        }
    }

    private fun toggleVRMode() {
        player?.getController(VRController::class.java)?.let { vr ->
            isVRModeEnabled = !isVRModeEnabled
            vr.enableVRMode(isVRModeEnabled)
        } ?: run {
            isVRModeEnabled = !isVRModeEnabled
        }
    }
    private fun loadMedia(videoUrl: HashMap<*, *>, result: MethodChannel.Result?) {
        this.videoUrl = videoUrl
        this.mediaEntry = createMediaEntry(videoUrl)

        player?.setMedia(mediaEntry!!)
        result?.success(true)
    }

    private fun createMediaEntry(videoUrl: HashMap<*, *>): PKMediaEntry {
        //Create media entry.
        val mediaEntry = PKMediaEntry()

        //Set id for the entry.
        mediaEntry.id = "testEntry"

        // Set media entry type. It could be Live,Vod or Unknown.
        // In this sample we use Vod.
        mediaEntry.setIsVRMediaType(true)
        mediaEntry.mediaType = PKMediaEntry.MediaEntryType.Vod

        // Create list that contains at least 1 media source.
        // Each media entry can contain a couple of different media sources.
        // All of them represent the same content, the difference is in it format.
        // For example same entry can contain PKMediaSource with dash and another
        // PKMediaSource can be with hls. The player will decide by itself which source is
        // preferred for playback.
        val mediaSources = createMediaSources(videoUrl)

        //Set media sources to the entry.
        mediaEntry.sources = mediaSources

        return mediaEntry
    }


    /**
     * Create list of [PKMediaSource].
     *
     * @return - the list of sources.
     */
    private fun createMediaSources(videoUrl: HashMap<*, *>): List<PKMediaSource> {

        // Create new PKMediaSource instance.
        val mediaSource = PKMediaSource()

        // Set the id.
        mediaSource.id = "testSource"

        // Set the content url. In our case it will be link to hls source(.m3u8).
        // Set the format of the source.
        // In our case it will be hls
        // in case of mpd/wvm formats you have to to call mediaSource.setDrmData method as well
        if (videoUrl.get("videoPath") != null){
            val url = videoUrl.get("videoPath") as String
            val extension = url.substringAfterLast('.', "");
            mediaSource.url = url
            mediaSource.mediaFormat = if(extension=="mp4") PKMediaFormat.mp4 else PKMediaFormat.hls
        } else {
            val url = videoUrl.get("videoUrl") as String
            val extension = url.substringAfterLast('.', "");
            mediaSource.url = url
            mediaSource.mediaFormat = if(extension=="mp4") PKMediaFormat.mp4 else PKMediaFormat.hls
        }

        return listOf(mediaSource)
    }

    private fun buildMediaOptions(entryId: String): OVPMediaOptions {
        val ovpMediaAsset = OVPMediaAsset()
        ovpMediaAsset.entryId = entryId
        ovpMediaAsset.ks = null
        val ovpMediaOptions = OVPMediaOptions(ovpMediaAsset)
        ovpMediaOptions.startPosition = 0
        return ovpMediaOptions
    }

    fun loadPlayer() {
        loadPlayerInner()
    }

    private fun loadPlayerInner() {
        if (player != null) return

        val playerInitOptions = PlayerInitOptions()
        playerInitOptions.setAutoPlay(false)
        playerInitOptions.setVRSettings(configureVRSettings())

        player = KalturaBasicPlayer.create(context, playerInitOptions).apply {
            setPlayerView(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            )
            listener.onViewCreated(playerView)

            addListener(this)
        }
    }

    private fun addListener(basicPlayer: KalturaBasicPlayer) {
        basicPlayer.addListener<PlayerEvent.StateChanged>(this, PlayerEvent.stateChanged) { event ->
            when (event.newState) {
                PlayerState.READY -> {
                    videoPlayerState = videoPlayerState?.let {
                        basicPlayer.seekTo(it.currentPosition)
                        isVRModeEnabled = true
                        toggleVRMode()
                        if (it.isPlaying) basicPlayer.play()
                        null
                    }
                    playerEventStateChanged?.success(mapOf(Pair("state", 1)))
                }
                PlayerState.LOADING -> {
                    playerEventStateChanged?.success(mapOf(Pair("state", 0)))
                }
                PlayerState.BUFFERING -> {
                    playerEventStateChanged?.success(mapOf(Pair("state", 2)))
                }
                else -> {}
            }
        }

        basicPlayer.addListener<PlayerEvent.PlayheadUpdated>(
            this,
            PlayerEvent.playheadUpdated
        ) { progressUpdate ->
            playerEventPosition?.success(mapOf(Pair("currentPosition", progressUpdate.position)))
        }

        basicPlayer.addListener<PlayerEvent.DurationChanged>(
            this,
            PlayerEvent.durationChanged
        ) { mediaDuration ->
            playerEventDurationChanged?.success(mapOf(Pair("duration", mediaDuration.duration)))
        }

        basicPlayer.addListener(this, PlayerEvent.ended) {
            playerEventEnded?.success(mapOf(Pair("ended", true)))
        }
    }

    private fun configureVRSettings(): VRSettings {
        val vrSettings = VRSettings()
        vrSettings.isFlingEnabled = true
        vrSettings.isVrModeEnabled = false
        vrSettings.interactionMode = VRInteractionMode.MotionWithTouch

        vrSettings.isZoomWithPinchEnabled = true
        return vrSettings
    }

    fun dispose() {
        player?.let {
            this.videoPlayerState = VideoPlayerState(it.currentPosition, it.isPlaying)
            it.destroy()
        }
        player = null

        playerEventStateChanged = null
        playerEventDurationChanged = null
        playerEventPosition = null
        playerEventEnded = null
    }

    inner class VideoPlayerState(val currentPosition: Long = 0, val isPlaying: Boolean = false)
}