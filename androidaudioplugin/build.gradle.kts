plugins {
    id ("com.android.library")
    id ("kotlin-android")
    id ("org.jetbrains.dokka")
    id ("maven-publish")
    id ("signing")
}

apply { from ("../common.gradle") }

// What a mess...
val kotlin_version: String by rootProject
val dokka_version: String by rootProject
val compose_version: String by rootProject
val aap_version: String by rootProject
val enable_asan: Boolean by rootProject

android {
    this.ext["description"] = "AndroidAudioPlugin - core"

    defaultConfig {
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        consumerProguardFiles ("consumer-rules.pro")

        externalNativeBuild {
            cmake {
                // https://github.com/google/prefab/blob/bccf5a6a75b67add30afbb6d4f7a7c50081d2d86/api/src/main/kotlin/com/google/prefab/api/Android.kt#L243
                arguments ("-DANDROID_STL=c++_shared", "-DAAP_ENABLE_ASAN=" + (if (enable_asan) "1" else "0"))
            }
        }
    }

    buildTypes {
        debug {
            packagingOptions {
                jniLibs.keepDebugSymbols.add("**/*.so")
            }
            externalNativeBuild {
                cmake {
                    cppFlags ("-Werror")
                }
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles (
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    externalNativeBuild {
        cmake {
            version = "3.22.1"
            path ("src/main/cpp/CMakeLists.txt")
        }
    }

    lint {
        disable.add("EnsureInitializerMetadata")
    }

    buildFeatures {
        prefabPublishing = true
    }
    prefab {
        create("androidaudioplugin") {
            name = "androidaudioplugin"
            headers = "../include"
        }
    }

    // https://github.com/google/prefab/issues/127
    packagingOptions {
        jniLibs.excludes.add("**/libc++_shared.so")
    }

    // FIXME: it is annoying to copy this everywhere, but build.gradle.kts is incapable of importing this fragment...
    // It's been long time until I got this working, and I have no idea why it started working.
    //  If you don't get this working, you are not alone: https://github.com/atsushieno/android-audio-plugin-framework/issues/85
    // Also note that you have to use custom sdk channel so far: ./gradlew testDevice1DebugAndroidTest -Pandroid.sdk.channel=3
    testOptions {
        devices {
            this.register<com.android.build.api.dsl.ManagedVirtualDevice>("testDevice1") {
                device = "Pixel 5"
                apiLevel = 30
                systemImageSource = "aosp-atd"
                //abi = "x86"
            }
        }
    }
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation ("androidx.core:core-ktx:1.7.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.6.1")
    implementation ("androidx.startup:startup-runtime:1.1.1")
    implementation ("org.jetbrains.kotlin:kotlin-stdlib-jdk7:$kotlin_version")
    testImplementation ("junit:junit:4.13.2")
    androidTestImplementation ("androidx.test:core:1.4.0")
    androidTestImplementation ("androidx.test:rules:1.4.0")
    androidTestImplementation ("androidx.test:runner:1.4.0")
    androidTestImplementation ("androidx.test.ext:junit:1.1.3")
    androidTestImplementation ("androidx.test.espresso:espresso-core:3.4.0")
}
