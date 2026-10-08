# AnyiOS implementation map

The percentages below count only explicitly inventoried candidate API exports and compatibility gates. They are **not percentages of all iOS functionality or playable apps**. Partial work does not count as complete.

| Metric | Verified | Partial | Pending or unverified | Tracked | Coverage |
| --- | ---: | ---: | ---: | ---: | ---: |
| iOS API exports | 3 | 1 | 265 | 269 | 1.12% |
| Compatibility gates | 6 | 10 | 223 | 239 | 2.51% |

## Candidate API exports

<details><summary><b>Audio / AVFoundation</b> — 0 verified, 0 partial, 13 pending / 13 total</summary>

- ⬜ `_AudioComponentFindNext` — Pending / unverified
- ⬜ `_AudioComponentInstanceNew` — Pending / unverified
- ⬜ `_AudioOutputUnitStart` — Pending / unverified
- ⬜ `_AudioQueueEnqueueBuffer` — Pending / unverified
- ⬜ `_AudioQueueNewOutput` — Pending / unverified
- ⬜ `_AudioQueueStart` — Pending / unverified
- ⬜ `_AudioQueueStop` — Pending / unverified
- ⬜ `_AudioUnitInitialize` — Pending / unverified
- ⬜ `_ExtAudioFileOpenURL` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_AVAudioEngine` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_AVAudioPlayer` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_AVAudioSession` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_AVPlayer` — Pending / unverified

</details>

<details><summary><b>CoreFoundation</b> — 0 verified, 0 partial, 15 pending / 15 total</summary>

- ⬜ `_CFArrayCreate` — Pending / unverified
- ⬜ `_CFArrayGetCount` — Pending / unverified
- ⬜ `_CFBundleGetMainBundle` — Pending / unverified
- ⬜ `_CFDataCreate` — Pending / unverified
- ⬜ `_CFDictionaryCreate` — Pending / unverified
- ⬜ `_CFDictionaryGetValue` — Pending / unverified
- ⬜ `_CFNotificationCenterGetLocalCenter` — Pending / unverified
- ⬜ `_CFRelease` — Pending / unverified
- ⬜ `_CFRetain` — Pending / unverified
- ⬜ `_CFRunLoopGetMain` — Pending / unverified
- ⬜ `_CFRunLoopRun` — Pending / unverified
- ⬜ `_CFStringCreateWithCString` — Pending / unverified
- ⬜ `_CFStringGetCString` — Pending / unverified
- ⬜ `_CFStringGetLength` — Pending / unverified
- ⬜ `_CFURLCreateWithFileSystemPath` — Pending / unverified

</details>

<details><summary><b>CoreGraphics / text</b> — 0 verified, 0 partial, 19 pending / 19 total</summary>

- ⬜ `_CGBitmapContextCreate` — Pending / unverified
- ⬜ `_CGColorCreate` — Pending / unverified
- ⬜ `_CGColorRelease` — Pending / unverified
- ⬜ `_CGColorSpaceCreateDeviceRGB` — Pending / unverified
- ⬜ `_CGContextConcatCTM` — Pending / unverified
- ⬜ `_CGContextDrawImage` — Pending / unverified
- ⬜ `_CGContextFillRect` — Pending / unverified
- ⬜ `_CGContextSetFillColorWithColor` — Pending / unverified
- ⬜ `_CGContextSetLineWidth` — Pending / unverified
- ⬜ `_CGContextSetStrokeColorWithColor` — Pending / unverified
- ⬜ `_CGContextStrokePath` — Pending / unverified
- ⬜ `_CGDataProviderCreateWithData` — Pending / unverified
- ⬜ `_CGImageCreate` — Pending / unverified
- ⬜ `_CGImageRelease` — Pending / unverified
- ⬜ `_CGPathAddLineToPoint` — Pending / unverified
- ⬜ `_CGPathCreateMutable` — Pending / unverified
- ⬜ `_CGPathRelease` — Pending / unverified
- ⬜ `_CTFontCreateWithName` — Pending / unverified
- ⬜ `_CTLineCreateWithAttributedString` — Pending / unverified

</details>

<details><summary><b>Darwin dispatch / blocks</b> — 0 verified, 0 partial, 13 pending / 13 total</summary>

- ⬜ `__Block_copy` — Pending / unverified
- ⬜ `__Block_release` — Pending / unverified
- ⬜ `_dispatch_after` — Pending / unverified
- ⬜ `_dispatch_async` — Pending / unverified
- ⬜ `_dispatch_group_create` — Pending / unverified
- ⬜ `_dispatch_group_enter` — Pending / unverified
- ⬜ `_dispatch_group_leave` — Pending / unverified
- ⬜ `_dispatch_once_f` — Pending / unverified
- ⬜ `_dispatch_queue_create` — Pending / unverified
- ⬜ `_dispatch_semaphore_create` — Pending / unverified
- ⬜ `_dispatch_semaphore_signal` — Pending / unverified
- ⬜ `_dispatch_semaphore_wait` — Pending / unverified
- ⬜ `_dispatch_sync` — Pending / unverified

</details>

<details><summary><b>Darwin file / process</b> — 1 verified, 0 partial, 25 pending / 26 total</summary>

- ✅ `_write` — Verified
- ⬜ `_access` — Pending / unverified
- ⬜ `_clock_gettime` — Pending / unverified
- ⬜ `_close` — Pending / unverified
- ⬜ `_fstat` — Pending / unverified
- ⬜ `_getcwd` — Pending / unverified
- ⬜ `_getenv` — Pending / unverified
- ⬜ `_getpid` — Pending / unverified
- ⬜ `_getppid` — Pending / unverified
- ⬜ `_gettimeofday` — Pending / unverified
- ⬜ `_lseek` — Pending / unverified
- ⬜ `_mkdir` — Pending / unverified
- ⬜ `_mmap` — Pending / unverified
- ⬜ `_mprotect` — Pending / unverified
- ⬜ `_munmap` — Pending / unverified
- ⬜ `_nanosleep` — Pending / unverified
- ⬜ `_open` — Pending / unverified
- ⬜ `_openat` — Pending / unverified
- ⬜ `_read` — Pending / unverified
- ⬜ `_rename` — Pending / unverified
- ⬜ `_rmdir` — Pending / unverified
- ⬜ `_setenv` — Pending / unverified
- ⬜ `_stat` — Pending / unverified
- ⬜ `_sysctl` — Pending / unverified
- ⬜ `_sysctlbyname` — Pending / unverified
- ⬜ `_unlink` — Pending / unverified

</details>

<details><summary><b>Darwin memory / strings</b> — 2 verified, 0 partial, 17 pending / 19 total</summary>

- ✅ `_exit` — Verified
- ✅ `_malloc` — Verified
- ⬜ `_abort` — Pending / unverified
- ⬜ `_bzero` — Pending / unverified
- ⬜ `_calloc` — Pending / unverified
- ⬜ `_free` — Pending / unverified
- ⬜ `_memcmp` — Pending / unverified
- ⬜ `_memcpy` — Pending / unverified
- ⬜ `_memmove` — Pending / unverified
- ⬜ `_memset` — Pending / unverified
- ⬜ `_realloc` — Pending / unverified
- ⬜ `_strchr` — Pending / unverified
- ⬜ `_strcmp` — Pending / unverified
- ⬜ `_strdup` — Pending / unverified
- ⬜ `_strlcat` — Pending / unverified
- ⬜ `_strlcpy` — Pending / unverified
- ⬜ `_strlen` — Pending / unverified
- ⬜ `_strncmp` — Pending / unverified
- ⬜ `_strstr` — Pending / unverified

</details>

<details><summary><b>Darwin threads / locks</b> — 0 verified, 1 partial, 12 pending / 13 total</summary>

- 🟨 `__tlv_bootstrap` — Partial
- ⬜ `_pthread_cond_signal` — Pending / unverified
- ⬜ `_pthread_cond_wait` — Pending / unverified
- ⬜ `_pthread_create` — Pending / unverified
- ⬜ `_pthread_getspecific` — Pending / unverified
- ⬜ `_pthread_join` — Pending / unverified
- ⬜ `_pthread_key_create` — Pending / unverified
- ⬜ `_pthread_mutex_init` — Pending / unverified
- ⬜ `_pthread_mutex_lock` — Pending / unverified
- ⬜ `_pthread_mutex_unlock` — Pending / unverified
- ⬜ `_pthread_once` — Pending / unverified
- ⬜ `_pthread_self` — Pending / unverified
- ⬜ `_pthread_setspecific` — Pending / unverified

</details>

<details><summary><b>Foundation classes</b> — 0 verified, 0 partial, 20 pending / 20 total</summary>

- ⬜ `_OBJC_CLASS_$_NSArray` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSBundle` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSData` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSDate` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSDictionary` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSError` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSFileManager` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSJSONSerialization` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSMutableArray` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSMutableDictionary` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSMutableString` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSNotificationCenter` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSNumber` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSObject` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSRunLoop` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSSet` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSString` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSTimer` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSURL` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSUserDefaults` — Pending / unverified

</details>

<details><summary><b>GameKit / stores</b> — 0 verified, 0 partial, 7 pending / 7 total</summary>

- ⬜ `_OBJC_CLASS_$_GKAchievement` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_GKLeaderboard` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_GKLocalPlayer` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_GKMatch` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_SKPaymentQueue` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_SKProduct` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_SKProductsRequest` — Pending / unverified

</details>

<details><summary><b>Metal / MetalKit</b> — 0 verified, 0 partial, 7 pending / 7 total</summary>

- ⬜ `_MTLCreateSystemDefaultDevice` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_CAMetalLayer` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_MTKView` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_MTLCompileOptions` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_MTLDepthStencilDescriptor` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_MTLRenderPassDescriptor` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_MTLTextureDescriptor` — Pending / unverified

</details>

<details><summary><b>Network / IPC</b> — 0 verified, 0 partial, 12 pending / 12 total</summary>

- ⬜ `_CFHTTPMessageCreateRequest` — Pending / unverified
- ⬜ `_CFReadStreamCreateForHTTPRequest` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSURLConnection` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSURLRequest` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSURLSession` — Pending / unverified
- ⬜ `_connect` — Pending / unverified
- ⬜ `_getaddrinfo` — Pending / unverified
- ⬜ `_poll` — Pending / unverified
- ⬜ `_recv` — Pending / unverified
- ⬜ `_select` — Pending / unverified
- ⬜ `_send` — Pending / unverified
- ⬜ `_socket` — Pending / unverified

</details>

<details><summary><b>Objective-C ABI</b> — 0 verified, 0 partial, 21 pending / 21 total</summary>

- ⬜ `_NSStringFromClass` — Pending / unverified
- ⬜ `_class_getInstanceMethod` — Pending / unverified
- ⬜ `_class_getSuperclass` — Pending / unverified
- ⬜ `_objc_allocateClassPair` — Pending / unverified
- ⬜ `_objc_autorelease` — Pending / unverified
- ⬜ `_objc_autoreleasePoolPop` — Pending / unverified
- ⬜ `_objc_autoreleasePoolPush` — Pending / unverified
- ⬜ `_objc_getAssociatedObject` — Pending / unverified
- ⬜ `_objc_getClass` — Pending / unverified
- ⬜ `_objc_getMetaClass` — Pending / unverified
- ⬜ `_objc_loadWeakRetained` — Pending / unverified
- ⬜ `_objc_msgSend` — Pending / unverified
- ⬜ `_objc_msgSendSuper2` — Pending / unverified
- ⬜ `_objc_registerClassPair` — Pending / unverified
- ⬜ `_objc_release` — Pending / unverified
- ⬜ `_objc_retain` — Pending / unverified
- ⬜ `_objc_setAssociatedObject` — Pending / unverified
- ⬜ `_objc_storeStrong` — Pending / unverified
- ⬜ `_objc_storeWeak` — Pending / unverified
- ⬜ `_object_getClass` — Pending / unverified
- ⬜ `_sel_registerName` — Pending / unverified

</details>

<details><summary><b>OpenGL ES</b> — 0 verified, 0 partial, 17 pending / 17 total</summary>

- ⬜ `_glBindTexture` — Pending / unverified
- ⬜ `_glClear` — Pending / unverified
- ⬜ `_glClearColor` — Pending / unverified
- ⬜ `_glCompileShader` — Pending / unverified
- ⬜ `_glCreateProgram` — Pending / unverified
- ⬜ `_glCreateShader` — Pending / unverified
- ⬜ `_glDrawArrays` — Pending / unverified
- ⬜ `_glDrawElements` — Pending / unverified
- ⬜ `_glEnableVertexAttribArray` — Pending / unverified
- ⬜ `_glGenTextures` — Pending / unverified
- ⬜ `_glGetError` — Pending / unverified
- ⬜ `_glGetString` — Pending / unverified
- ⬜ `_glLinkProgram` — Pending / unverified
- ⬜ `_glTexImage2D` — Pending / unverified
- ⬜ `_glUseProgram` — Pending / unverified
- ⬜ `_glVertexAttribPointer` — Pending / unverified
- ⬜ `_glViewport` — Pending / unverified

</details>

<details><summary><b>Persistence / CloudKit</b> — 0 verified, 0 partial, 10 pending / 10 total</summary>

- ⬜ `_OBJC_CLASS_$_CKContainer` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_CKDatabase` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSManagedObjectContext` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_NSPersistentStoreCoordinator` — Pending / unverified
- ⬜ `_sqlite3_bind_text` — Pending / unverified
- ⬜ `_sqlite3_close` — Pending / unverified
- ⬜ `_sqlite3_finalize` — Pending / unverified
- ⬜ `_sqlite3_open` — Pending / unverified
- ⬜ `_sqlite3_prepare_v2` — Pending / unverified
- ⬜ `_sqlite3_step` — Pending / unverified

</details>

<details><summary><b>QuartzCore</b> — 0 verified, 0 partial, 8 pending / 8 total</summary>

- ⬜ `_CACurrentMediaTime` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_CAAnimation` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_CABasicAnimation` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_CADisplayLink` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_CAKeyframeAnimation` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_CALayer` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_CAMediaTimingFunction` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_CATransaction` — Pending / unverified

</details>

<details><summary><b>Security / keychain</b> — 0 verified, 0 partial, 9 pending / 9 total</summary>

- ⬜ `_CCCrypt` — Pending / unverified
- ⬜ `_CC_SHA256` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_LAContext` — Pending / unverified
- ⬜ `_SecItemAdd` — Pending / unverified
- ⬜ `_SecItemCopyMatching` — Pending / unverified
- ⬜ `_SecItemDelete` — Pending / unverified
- ⬜ `_SecItemUpdate` — Pending / unverified
- ⬜ `_SecKeyCreateRandomKey` — Pending / unverified
- ⬜ `_SecRandomCopyBytes` — Pending / unverified

</details>

<details><summary><b>SpriteKit</b> — 0 verified, 0 partial, 9 pending / 9 total</summary>

- ⬜ `_OBJC_CLASS_$_SKAction` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_SKEmitterNode` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_SKLabelNode` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_SKNode` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_SKPhysicsBody` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_SKScene` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_SKSpriteNode` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_SKTexture` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_SKView` — Pending / unverified

</details>

<details><summary><b>Swift ABI</b> — 0 verified, 0 partial, 11 pending / 11 total</summary>

- ⬜ `_swift_allocObject` — Pending / unverified
- ⬜ `_swift_beginAccess` — Pending / unverified
- ⬜ `_swift_bridgeObjectRelease` — Pending / unverified
- ⬜ `_swift_bridgeObjectRetain` — Pending / unverified
- ⬜ `_swift_deallocClassInstance` — Pending / unverified
- ⬜ `_swift_endAccess` — Pending / unverified
- ⬜ `_swift_getTypeByMangledNameInContext` — Pending / unverified
- ⬜ `_swift_getWitnessTable` — Pending / unverified
- ⬜ `_swift_once` — Pending / unverified
- ⬜ `_swift_release` — Pending / unverified
- ⬜ `_swift_retain` — Pending / unverified

</details>

<details><summary><b>UIKit lifecycle / views</b> — 0 verified, 0 partial, 20 pending / 20 total</summary>

- ⬜ `_OBJC_CLASS_$_UIApplication` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIApplicationDelegate` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIButton` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UICollectionView` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIGestureRecognizer` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIImage` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIImageView` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UILabel` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UINavigationController` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIResponder` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIScreen` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIStackView` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIStoryboard` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UITableView` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UITextField` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UITouch` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIView` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIViewController` — Pending / unverified
- ⬜ `_OBJC_CLASS_$_UIWindow` — Pending / unverified
- ⬜ `_UIApplicationMain` — Pending / unverified

</details>


## Runtime/framework/Windows gates

<details><summary><b>App capabilities</b> — 0 verified, 0 partial, 8 pending / 8 total</summary>

- ⬜ `Background tasks and scheduling` — Pending / unverified
- ⬜ `Camera, photo library and media selection` — Pending / unverified
- ⬜ `Files picker and document providers` — Pending / unverified
- ⬜ `HealthKit, contacts and calendars` — Pending / unverified
- ⬜ `Location and map views` — Pending / unverified
- ⬜ `Push notifications / APNs` — Pending / unverified
- ⬜ `Universal links and URL handlers` — Pending / unverified
- ⬜ `WidgetKit and app extensions` — Pending / unverified

</details>

<details><summary><b>ARM64 CPU / ABI</b> — 1 verified, 2 partial, 6 pending / 9 total</summary>

- ✅ `Windows x86-64 Dynarmic owned ARM64 instructions` — Verified
- 🟨 `Darwin SVC trap and Mach syscall handling` — Partial
- 🟨 `Host-to-guest fixed-argument ABI callbacks` — Partial
- ⬜ `Exception, unwind and stack walkers` — Pending / unverified
- ⬜ `Float/vector NEON and SIMD across callbacks` — Pending / unverified
- ⬜ `Hardware atomics and memory ordering` — Pending / unverified
- ⬜ `Instruction cache invalidation and self-modifying code` — Pending / unverified
- ⬜ `MRS/MSR platform register virtualization` — Pending / unverified
- ⬜ `Variadic calls, structs and ABI thunks` — Pending / unverified

</details>

<details><summary><b>Audio / media</b> — 0 verified, 0 partial, 8 pending / 8 total</summary>

- ⬜ `AVAudioEngine and playback APIs` — Pending / unverified
- ⬜ `AVPlayer video playback, synchronization` — Pending / unverified
- ⬜ `Audio session categories and route events` — Pending / unverified
- ⬜ `AudioUnits and callback/render graph` — Pending / unverified
- ⬜ `CoreAudio device abstraction and timing` — Pending / unverified
- ⬜ `Hardware volume and Bluetooth behavior` — Pending / unverified
- ⬜ `Microphone capture and permission mapping` — Pending / unverified
- ⬜ `WAV/MP3/AAC/ALAC decoding` — Pending / unverified

</details>

<details><summary><b>Compatibility tooling</b> — 1 verified, 0 partial, 8 pending / 9 total</summary>

- ✅ `Strict Mach-O inspection and import-gap report` — Verified
- ⬜ `Crash trace attribution and deterministic reproduction` — Pending / unverified
- ⬜ `Function-level regression tests and stub classification` — Pending / unverified
- ⬜ `Golden screenshots and event replay` — Pending / unverified
- ⬜ `Original IPA/.app metadata-only intake` — Pending / unverified
- ⬜ `Per-framework unimplemented function catalog` — Pending / unverified
- ⬜ `Unmodified app launch+render+input evidence gates` — Pending / unverified
- ⬜ `Windows ARM64 CI and artifact packaging` — Pending / unverified
- ⬜ `Windows x64 CI and artifact packaging` — Pending / unverified

</details>

<details><summary><b>CoreFoundation</b> — 0 verified, 0 partial, 7 pending / 7 total</summary>

- ⬜ `CFArray/CFDictionary/CFSet ownership` — Pending / unverified
- ⬜ `CFBundle/CFURL/CFPreferences` — Pending / unverified
- ⬜ `CFData/CFNumber/CFBoolean and date` — Pending / unverified
- ⬜ `CFError and bridging with Foundation` — Pending / unverified
- ⬜ `CFNotificationCenter and observers` — Pending / unverified
- ⬜ `CFRunLoop and source/timer scheduling` — Pending / unverified
- ⬜ `CFString and mutable strings` — Pending / unverified

</details>

<details><summary><b>CoreGraphics / CoreText</b> — 0 verified, 0 partial, 7 pending / 7 total</summary>

- ⬜ `2D compositing, clipping and performance` — Pending / unverified
- ⬜ `Bitmap, JPEG/PNG asset decode and scaling` — Pending / unverified
- ⬜ `CGContext and image drawing` — Pending / unverified
- ⬜ `Color management and HiDPI output` — Pending / unverified
- ⬜ `Color spaces and alpha blending` — Pending / unverified
- ⬜ `Font shaping, text glyphs and CoreText` — Pending / unverified
- ⬜ `Paths, clipping, transforms and strokes` — Pending / unverified

</details>

<details><summary><b>Darwin libSystem</b> — 0 verified, 2 partial, 7 pending / 9 total</summary>

- 🟨 `malloc/write/exit guest C fixture ABI` — Partial
- 🟨 `process arguments/envp/apple arrays` — Partial
- ⬜ `Full memcpy/memmove/memset and string semantics` — Pending / unverified
- ⬜ `calloc/realloc/free and aligned alloc` — Pending / unverified
- ⬜ `clocks, timers, gettimeofday and sysctl` — Pending / unverified
- ⬜ `errno and libc thread state` — Pending / unverified
- ⬜ `posix files/read/write/seek/stat` — Pending / unverified
- ⬜ `sockets and POSIX DNS` — Pending / unverified
- ⬜ `stdio, snprintf/vsnprintf/formatting` — Pending / unverified

</details>

<details><summary><b>Data and persistence</b> — 0 verified, 0 partial, 6 pending / 6 total</summary>

- ⬜ `App sandbox documents and caches` — Pending / unverified
- ⬜ `Core Data contexts and model migration` — Pending / unverified
- ⬜ `Resource bundles and localized assets` — Pending / unverified
- ⬜ `SQLite bundled API coverage` — Pending / unverified
- ⬜ `Save state persistence and corruption recovery` — Pending / unverified
- ⬜ `iCloud/CloudKit record and sync API` — Pending / unverified

</details>

<details><summary><b>Distribution / platform</b> — 0 verified, 0 partial, 7 pending / 7 total</summary>

- ⬜ `App-specific optional user supplied assets only` — Pending / unverified
- ⬜ `Auto-update, error reporting and diagnostics` — Pending / unverified
- ⬜ `Bundled redistributable OSS license notices` — Pending / unverified
- ⬜ `Installation, PATH/file associations and drag-drop` — Pending / unverified
- ⬜ `Real-world GPU/CPU compatibility test matrix` — Pending / unverified
- ⬜ `Self-contained Windows x86-64 release package` — Pending / unverified
- ⬜ `Windows ARM64 native runtime release package` — Pending / unverified

</details>

<details><summary><b>dyld and linking</b> — 0 verified, 1 partial, 8 pending / 9 total</summary>

- 🟨 `Chained rebases and binds for owned fixtures` — Partial
- ⬜ `App/framework nested bundle resolution` — Pending / unverified
- ⬜ `C/C++ static constructors across images` — Pending / unverified
- ⬜ `Classic binds, lazy binds and weak symbols` — Pending / unverified
- ⬜ `Multi-dylib recursive load and @rpath resolution` — Pending / unverified
- ⬜ `Objective-C category/selector initialization order` — Pending / unverified
- ⬜ `Stable ASLR and image slides` — Pending / unverified
- ⬜ `Thread local variable fixups across modules` — Pending / unverified
- ⬜ `dyld shared cache equivalent / substitute frameworks` — Pending / unverified

</details>

<details><summary><b>Foundation</b> — 0 verified, 0 partial, 10 pending / 10 total</summary>

- ⬜ `JSON parsing, serialization and regex` — Pending / unverified
- ⬜ `NSBundle resources and localization` — Pending / unverified
- ⬜ `NSCoder, archiving, keyed storage` — Pending / unverified
- ⬜ `NSData, NSNumber, NSDate, NSURL objects` — Pending / unverified
- ⬜ `NSError, exceptions and NSNotificationCenter` — Pending / unverified
- ⬜ `NSFileManager, URLs and path semantics` — Pending / unverified
- ⬜ `NSOperationQueue and async APIs` — Pending / unverified
- ⬜ `NSRunLoop, NSTimer and observers` — Pending / unverified
- ⬜ `NSString, NSArray, NSDictionary classes` — Pending / unverified
- ⬜ `NSUserDefaults and key-value persistence` — Pending / unverified

</details>

<details><summary><b>Game engine integration</b> — 0 verified, 0 partial, 6 pending / 6 total</summary>

- ⬜ `AssetBundles, compressed game assets and codecs` — Pending / unverified
- ⬜ `Dynamic library and plugin sandbox` — Pending / unverified
- ⬜ `Frame pacing and shader-cache behavior` — Pending / unverified
- ⬜ `Unity IL2CPP and Mono runtime compatibility` — Pending / unverified
- ⬜ `Unreal engine iOS build architecture` — Pending / unverified
- ⬜ `cocos2d and native C++ engine support` — Pending / unverified

</details>

<details><summary><b>GameKit / commerce</b> — 0 verified, 0 partial, 6 pending / 6 total</summary>

- ⬜ `Apple Arcade entitlement and licensing constraints` — Pending / unverified
- ⬜ `Cloud saves and cross-device state` — Pending / unverified
- ⬜ `Game Center sign-in and achievements` — Pending / unverified
- ⬜ `In-app purchases and StoreKit receipts` — Pending / unverified
- ⬜ `Leaderboards, match and invites` — Pending / unverified
- ⬜ `Subscription and entitlement validation` — Pending / unverified

</details>

<details><summary><b>Guest memory / sandbox</b> — 1 verified, 1 partial, 7 pending / 9 total</summary>

- ✅ `Input-binary validation and refusal of protected code` — Verified
- 🟨 `Bounded W^X guest memory ranges` — Partial
- ⬜ `App sandbox path translation` — Pending / unverified
- ⬜ `Crash isolation from host UI` — Pending / unverified
- ⬜ `Host resource limits and watchdogs` — Pending / unverified
- ⬜ `Large address-space sparse virtual memory` — Pending / unverified
- ⬜ `Memory allocation/free/heap fragmentation` — Pending / unverified
- ⬜ `Virtual filesystem and document directories` — Pending / unverified
- ⬜ `mmap, mprotect and guard pages` — Pending / unverified

</details>

<details><summary><b>Input / controllers</b> — 0 verified, 0 partial, 7 pending / 7 total</summary>

- ⬜ `Accelerometer/gyro synthetic sensors` — Pending / unverified
- ⬜ `GameController.framework profiles and analog axes` — Pending / unverified
- ⬜ `Gamepad reconnect and focus behavior` — Pending / unverified
- ⬜ `Haptics/CoreHaptics vibration mapping` — Pending / unverified
- ⬜ `Keyboard, on-screen text and IME mapping` — Pending / unverified
- ⬜ `Multi-touch gestures, drag and pinch` — Pending / unverified
- ⬜ `Windows mouse to UITouch translation` — Pending / unverified

</details>

<details><summary><b>Legacy OpenGL ES</b> — 0 verified, 0 partial, 6 pending / 6 total</summary>

- ⬜ `Buffer objects and vertex attribute streams` — Pending / unverified
- ⬜ `EAGLContext and framebuffer setup` — Pending / unverified
- ⬜ `Extensions and driver capability query` — Pending / unverified
- ⬜ `GLES2/GLES3 shader translator` — Pending / unverified
- ⬜ `Textures, renderbuffers and framebuffer objects` — Pending / unverified
- ⬜ `Uniforms, draws, blending and depth` — Pending / unverified

</details>

<details><summary><b>Mach-O formats</b> — 2 verified, 0 partial, 6 pending / 8 total</summary>

- ✅ `Fat/FAT64 universal slice selection` — Verified
- ✅ `Thin ARM64 MH_EXECUTE and MH_DYLIB metadata` — Verified
- ⬜ `Encrypted binary detection and explicit rejection` — Pending / unverified
- ⬜ `LC_MAIN and LC_UNIXTHREAD bootstrap` — Pending / unverified
- ⬜ `Mach-O segment protections and page zero` — Pending / unverified
- ⬜ `Symbol table and export trie lookup` — Pending / unverified
- ⬜ `Versioned iOS platform and load-command variants` — Pending / unverified
- ⬜ `arm64e pointer authentication compatibility` — Pending / unverified

</details>

<details><summary><b>Metal graphics</b> — 0 verified, 0 partial, 9 pending / 9 total</summary>

- ⬜ `Compute dispatch and barriers` — Pending / unverified
- ⬜ `Depth/stencil and blend states` — Pending / unverified
- ⬜ `Indirect drawing and synchronization` — Pending / unverified
- ⬜ `MTLCommandQueue and command buffers` — Pending / unverified
- ⬜ `MTLDevice and GPU feature query` — Pending / unverified
- ⬜ `Metal shader libraries and MSL compilation` — Pending / unverified
- ⬜ `Metal swapchain via CAMetalLayer` — Pending / unverified
- ⬜ `Render pass and pipeline states` — Pending / unverified
- ⬜ `Textures, buffers, heaps and resources` — Pending / unverified

</details>

<details><summary><b>Networking / services</b> — 0 verified, 0 partial, 8 pending / 8 total</summary>

- ⬜ `Bonjour/mDNS discovery and LAN permission` — Pending / unverified
- ⬜ `CFNetwork TCP/TLS streams` — Pending / unverified
- ⬜ `NSURLSession HTTP(S) and URL loading` — Pending / unverified
- ⬜ `Network path change callbacks` — Pending / unverified
- ⬜ `Offline/cache and network reachability` — Pending / unverified
- ⬜ `POSIX sockets and DNS resolver` — Pending / unverified
- ⬜ `TLS certificate validation and pinning` — Pending / unverified
- ⬜ `WebSockets, background transfers and proxies` — Pending / unverified

</details>

<details><summary><b>Objective-C runtime</b> — 0 verified, 1 partial, 8 pending / 9 total</summary>

- 🟨 `Classlist/selector metadata inspection` — Partial
- ⬜ `Autorelease pool push/pop with actual objects` — Pending / unverified
- ⬜ `Categories, protocols and dynamic method lookup` — Pending / unverified
- ⬜ `Class registry, metaclasses and inheritance` — Pending / unverified
- ⬜ `Exceptions and Objective-C synchronized blocks` — Pending / unverified
- ⬜ `General objc_msgSend instance/class/super dispatch` — Pending / unverified
- ⬜ `KVO, blocks and associated objects` — Pending / unverified
- ⬜ `Runtime-generated class and method interop` — Pending / unverified
- ⬜ `alloc/init/retain/release/weak references` — Pending / unverified

</details>

<details><summary><b>Pthreads and dispatch</b> — 0 verified, 1 partial, 7 pending / 8 total</summary>

- 🟨 `Owned Darwin guest TLV sequential threads` — Partial
- ⬜ `Atomic callbacks across guest and host threads` — Pending / unverified
- ⬜ `Main-thread dispatch and runloop integration` — Pending / unverified
- ⬜ `Thread cancel, join and teardown` — Pending / unverified
- ⬜ `Thread-safe autorelease pools` — Pending / unverified
- ⬜ `Work queues/QoS scheduling` — Pending / unverified
- ⬜ `dispatch queues, groups, semaphores` — Pending / unverified
- ⬜ `pthread creation, mutex, condvar and once` — Pending / unverified

</details>

<details><summary><b>QuartzCore / animation</b> — 0 verified, 0 partial, 6 pending / 6 total</summary>

- ⬜ `Animation transactions and timing` — Pending / unverified
- ⬜ `CADisplayLink vsync callbacks` — Pending / unverified
- ⬜ `CALayer tree and compositing` — Pending / unverified
- ⬜ `Core Animation keyframes and easing` — Pending / unverified
- ⬜ `Layer masks, opacity and blending` — Pending / unverified
- ⬜ `Offscreen layer rendering and filters` — Pending / unverified

</details>

<details><summary><b>Security / crypto</b> — 0 verified, 0 partial, 7 pending / 7 total</summary>

- ⬜ `Biometric LocalAuthentication mapping` — Pending / unverified
- ⬜ `Code signing identity vs runtime permissions` — Pending / unverified
- ⬜ `CommonCrypto hashing and encryption` — Pending / unverified
- ⬜ `File protection and app container encryption` — Pending / unverified
- ⬜ `Secure random values and key APIs` — Pending / unverified
- ⬜ `Security.framework keychain APIs` — Pending / unverified
- ⬜ `Trust stores, certificates and entitlement policy` — Pending / unverified

</details>

<details><summary><b>SpriteKit / SceneKit</b> — 0 verified, 0 partial, 8 pending / 8 total</summary>

- ⬜ `SCNScene node graph and asset import` — Pending / unverified
- ⬜ `SKAction animations, scheduling and timing` — Pending / unverified
- ⬜ `SKEmitterNode particles and filters` — Pending / unverified
- ⬜ `SKNode hierarchy and transforms` — Pending / unverified
- ⬜ `SKPhysicsBody collision and contacts` — Pending / unverified
- ⬜ `SKSpriteNode sprite batching and texture atlas` — Pending / unverified
- ⬜ `SKView and SKScene lifecycle` — Pending / unverified
- ⬜ `SceneKit lighting, geometry and rendering` — Pending / unverified

</details>

<details><summary><b>Swift runtime</b> — 0 verified, 0 partial, 8 pending / 8 total</summary>

- ⬜ `Generics, existential containers and bridging` — Pending / unverified
- ⬜ `Swift ABI-version compatibility` — Pending / unverified
- ⬜ `Swift concurrency actors/task scheduler` — Pending / unverified
- ⬜ `Swift error handling, exceptions and unwinding` — Pending / unverified
- ⬜ `Swift metadata and type descriptors` — Pending / unverified
- ⬜ `Swift retain/release ownership` — Pending / unverified
- ⬜ `Swift/Objective-C mixed dispatch` — Pending / unverified
- ⬜ `Value witness and protocol witness tables` — Pending / unverified

</details>

<details><summary><b>System interoperability</b> — 0 verified, 1 partial, 7 pending / 8 total</summary>

- 🟨 `Process startup and exit lifecycle` — Partial
- ⬜ `Accessibility tree and screen readers` — Pending / unverified
- ⬜ `CFPreferences and persistent settings` — Pending / unverified
- ⬜ `Environment/version/device capabilities` — Pending / unverified
- ⬜ `Error reporting, logs and symbolication` — Pending / unverified
- ⬜ `Privacy prompts and permission persistence` — Pending / unverified
- ⬜ `Time zones, locale and encoding` — Pending / unverified
- ⬜ `XPC/Mach IPC and bootstrap services` — Pending / unverified

</details>

<details><summary><b>UIKit app lifecycle</b> — 0 verified, 0 partial, 8 pending / 8 total</summary>

- ⬜ `App foreground/background lifecycle` — Pending / unverified
- ⬜ `Root view controller and lifecycle callbacks` — Pending / unverified
- ⬜ `Status bar/appearance and accessibility hooks` — Pending / unverified
- ⬜ `Storyboard/NIB loading and object awakening` — Pending / unverified
- ⬜ `UIApplication event loop and scenes` — Pending / unverified
- ⬜ `UIApplicationMain and delegate construction` — Pending / unverified
- ⬜ `UIKit trait/environment state` — Pending / unverified
- ⬜ `UIWindow creation and visibility` — Pending / unverified

</details>

<details><summary><b>UIKit views and controls</b> — 0 verified, 0 partial, 9 pending / 9 total</summary>

- ⬜ `Animation, coordinate and hit-testing semantics` — Pending / unverified
- ⬜ `Clipboard, menus and context actions` — Pending / unverified
- ⬜ `Gestures, taps, touch dragging and multi-touch` — Pending / unverified
- ⬜ `Layout constraints and Auto Layout engine` — Pending / unverified
- ⬜ `Navigation, modal presentation and alerts` — Pending / unverified
- ⬜ `Scroll/table/collection views` — Pending / unverified
- ⬜ `Text input, keyboard and IME` — Pending / unverified
- ⬜ `UIButton, UILabel, UIImageView and text rendering` — Pending / unverified
- ⬜ `UIView hierarchy, transforms and layers` — Pending / unverified

</details>

<details><summary><b>Web / UI embeddings</b> — 0 verified, 0 partial, 5 pending / 5 total</summary>

- ⬜ `HTML forms, cookies and storage` — Pending / unverified
- ⬜ `JSCore guest/host JavaScript bridge` — Pending / unverified
- ⬜ `URL interception and custom schemes` — Pending / unverified
- ⬜ `WKWebView/WebKit DOM and JS engine` — Pending / unverified
- ⬜ `Web content process isolation` — Pending / unverified

</details>

<details><summary><b>Windows ARM64 execution</b> — 1 verified, 1 partial, 6 pending / 8 total</summary>

- ✅ `Trusted native ARM64 owned two-instruction fixture` — Verified
- 🟨 `Windows 11 ARM64 x64-emulated Dynarmic fallback` — Partial
- ⬜ `ABI interoperability and native exception safety` — Pending / unverified
- ⬜ `Cross-architecture same-app regression tests` — Pending / unverified
- ⬜ `Darwin service traps in native backend` — Pending / unverified
- ⬜ `General native guest Mach-O mapped executable pages` — Pending / unverified
- ⬜ `Guest TLS state isolation on native backend` — Pending / unverified
- ⬜ `Native ARM64 renderer/app integration` — Pending / unverified

</details>

<details><summary><b>Windows GUI backend</b> — 0 verified, 0 partial, 7 pending / 7 total</summary>

- ⬜ `Actual resizable Win32 window with guest-driven pixels` — Pending / unverified
- ⬜ `DPI scaling, fullscreen and multiple monitors` — Pending / unverified
- ⬜ `GDI fallbacks for test fixtures` — Pending / unverified
- ⬜ `Guest UI scene to Direct3D 11/12 compositor` — Pending / unverified
- ⬜ `Input hit testing, cursor and touch dispatch` — Pending / unverified
- ⬜ `Multi-window, swapchain and compositor resources` — Pending / unverified
- ⬜ `Presentation frame timing and resize` — Pending / unverified

</details>


Generated from [API exports](../tools/api_inventory.json), [API evidence](../tools/api_manifest.json), [runtime gates](../tools/compat_capabilities.json), and [counting rules](../_docs/PROGRESS.md).
