import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:vr_player/src/vr_player_controller.dart';
import 'package:vr_player/src/vr_player_created_callback.dart';
import 'package:vr_player/src/vr_player_observer.dart';
import 'package:vr_player/src/vr_state.dart';

class VrPlayer extends StatefulWidget {
  final VrPlayerCreatedCallback onCreated;
  final double x;
  final double y;

  /// Make sure that the best aspect ratio is 2:1
  /// https://developers.google.com/vr/discover/360-degree-media
  final double width;
  final double height;

  const VrPlayer({
    required this.onCreated,
    required this.x,
    required this.y,
    required this.width,
    required this.height,
    super.key,
  });

  @override
  State<StatefulWidget> createState() => _VideoPlayerState();
}

class _VideoPlayerState extends State<VrPlayer> with WidgetsBindingObserver {
  late VrPlayerController _videoPlayerController;
  late VrPlayerObserver _playerObserver;
  int? _textureId;
  bool _isTizen = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    if (!kIsWeb) {
      if (Platform.operatingSystem == 'tizen' || Platform.isLinux) {
        _isTizen = true;
        _initTizenPlayer();
      }
    }
  }

  Future<void> _initTizenPlayer() async {
    const channel = MethodChannel('vr_player');
    final textureId = await channel.invokeMethod<int>('init');
    if (textureId != null) {
      if (mounted) {
        setState(() {
          _textureId = textureId;
        });
        onPlatformViewCreated(textureId);
        // Emulate an early readiness signal for tizen specifically.
        _playerObserver.onStateChange?.call(VrState.ready);
      }
    }
  }

  @override
  void didUpdateWidget(VrPlayer oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.width != widget.width) {
      final pixelRatio =
          Platform.isAndroid ? MediaQuery.of(context).devicePixelRatio : 1;

      final width = widget.width * pixelRatio;
      final height = widget.height * pixelRatio;

      _videoPlayerController.onSizeChanged(width, height);
    }
  }

  @override
  Future<void> didChangeAppLifecycleState(AppLifecycleState state) async {
    if (state == AppLifecycleState.resumed) {
      await _videoPlayerController.onResume();
    } else if (state == AppLifecycleState.paused) {
      await _videoPlayerController.onPause();
    }
    super.didChangeAppLifecycleState(state);
  }

  @override
  void dispose() {
    _playerObserver.cancelListeners();
    _videoPlayerController
      ..pause()
      ..dispose();

    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    const viewType = 'plugins.vr_player/player_view';

    if (_isTizen) {
      return Container(
        color: Colors.black,
        width: widget.width,
        height: widget.height,
        child: _textureId != null
            ? Texture(textureId: _textureId!)
            : const SizedBox.shrink(),
      );
    }

    return Container(
      color: Colors.black,
      width: widget.width,
      height: widget.height,
      child: Platform.isAndroid
          ? PlatformViewLink(
              surfaceFactory: (context, controller) {
                return AndroidViewSurface(
                  controller: controller as AndroidViewController,
                  hitTestBehavior: PlatformViewHitTestBehavior.opaque,
                  gestureRecognizers: const <Factory<
                      OneSequenceGestureRecognizer>>{
                    Factory(TapGestureRecognizer.new),
                  },
                );
              },
              onCreatePlatformView: (params) {
                return PlatformViewsService.initExpensiveAndroidView(
                  id: params.id,
                  creationParams: {},
                  creationParamsCodec: const StandardMessageCodec(),
                  layoutDirection: TextDirection.ltr,
                  viewType: viewType,
                )
                  ..addOnPlatformViewCreatedListener(
                    params.onPlatformViewCreated,
                  )
                  ..addOnPlatformViewCreatedListener(onPlatformViewCreated)
                  ..create(size: Size(widget.width, widget.height));
              },
              viewType: viewType,
            )
          : UiKitView(
              viewType: viewType,
              onPlatformViewCreated: onPlatformViewCreated,
              creationParams: <String, dynamic>{
                'x': widget.x,
                'y': widget.y,
                'width': widget.width,
                'height': widget.height,
              },
              creationParamsCodec: const StandardMessageCodec(),
            ),
    );
  }

  void onPlatformViewCreated(int id) {
    _videoPlayerController = VrPlayerController.init(id);
    _playerObserver = VrPlayerObserver.init(id);
    widget.onCreated(_videoPlayerController, _playerObserver);
  }
}
