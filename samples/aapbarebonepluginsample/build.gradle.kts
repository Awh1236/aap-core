plugins {
    id ("com.android.application")
    id ("kotlin-android")
}

apply { from ("../../common.gradle") }

// What a mess...
val kotlin_version: String by rootProject
val dokka_version: String by rootProject
val compose_version: String by rootProject
val aap_version: String by rootProject
val enable_asan: Boolean by rootProject

android {
    defaultConfig {
        applicationId = "org.androidaudioplugin.aapbarebonepluginsample"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                arguments ("-DANDROID_STL=c++_shared")
            }
        }
    }
    // FIXME: PREFAB: enable these sections once we migrate to prefab-based solution.
    /*
    buildFeatures {
        prefab true
    }
    */
    externalNativeBuild {
        cmake {
            version = "3.18.1"
            path ("CMakeLists.txt")
        }
    }
    buildTypes {
        debug {
            packagingOptions {
                doNotStrip ("**/*.so")
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    packagingOptions {
        exclude ("META-INF/AL2.0")
        exclude ("META-INF/LGPL2.1")
        if (enable_asan)
            jniLibs.useLegacyPackaging = true
    }
}

dependencies {
    implementation ("androidx.core:core-ktx:1.7.0")
    implementation ("org.jetbrains.kotlin:kotlin-stdlib-jdk7:$kotlin_version")
    implementation ("androidx.appcompat:appcompat:1.4.1")
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-ui-compose"))

    androidTestImplementation ("androidx.test:rules:1.4.0")
    androidTestImplementation ("androidx.test.ext:junit:1.1.3")
    androidTestImplementation ("androidx.compose.ui:ui-test-junit4:$compose_version")
    androidTestImplementation (project(":androidaudioplugin-testing"))
}

// Starting AGP 7.0.0-alpha05, AGP stopped caring build dependencies and it broke builds.
// This is a forcible workarounds to build libandroidaudioplugin.so in prior to referencing it.
gradle.projectsEvaluated {
    tasks["buildCMakeDebug"].dependsOn(rootProject.project("androidaudioplugin").tasks["mergeDebugNativeLibs"])
    // It is super awkward in AGP or Android Studio, but gradlew --tasks does not list
    // `buildCMakeRelease` while `buildCMakeDebug` is there!
    // Fortunately(?) we can rely on another awkwardness that we reference *debug* version of
    //  -;androidaudioplugin in CMakeLists.txt for target_link_directories() so we can skip
    //  release dependency here (technically we need debug build *always* happen before
    //  release builds, but it always seems to happen.
//    tasks["buildCMakeRelease"].dependsOn(rootProject.project("androidaudioplugin").mergeReleaseNativeLibs)
}
