#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint vr_player.podspec' to validate before publishing.
#
Pod::Spec.new do |s|

  # ――― Spec Metadata ――― #
  s.name             = 'vr_player'
  s.version          = '0.0.1'
  s.summary          = 'VrPlayer Plugin for Flutter'
  s.description      = <<-DESC
  This is the VR framework for iOS devices.
                       DESC
  s.homepage         = 'http://example.com'

  # ――― Spec License ――― #
  s.license = { :type => 'AGPLv3', :file => '../LICENSE' }

  # ――― Author Metadata ―――#
  s.authors = {
    'Kaltura' => 'community@kaltura.com',
    'What the Flutter' => 'dev@flutter.wtf',
  }

  # ――― Platform Specifics ―――#
  s.platform = :ios, "9.0"
  s.ios.deployment_target = "9.0"

  # ――― Source Location ――― #
  s.source = { :path => '.' }

  # ――― Source Files ――― #
  s.source_files = 'Classes/**/*'

  # ――― Project Settings ――― #s
  s.swift_version = '5.0'
  s.dependency 'Flutter'
  s.dependency 'PlayKitVRCustomized'

  s.subspec 'PlayKitProvidersCustomized' do |ss|
    ss.source_files = 'PlayKitProvidersCustomized/**/*'
    ss.dependency 'KalturaNetKit', '~> 1.7.0'
    ss.dependency 'PlayKitUtils', '~> 0.7'
    ss.dependency 'SwiftyXMLParser', '5.6.0'
    ss.dependency 'PlayKitCustomized'
  end

end
