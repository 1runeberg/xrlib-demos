// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
@preconcurrency import CompositorServices
import SwiftUI
import Darwin
import Metal
import VisionOpenXR

struct ModelConfiguration: CompositorLayerConfiguration {
    func makeConfiguration(capabilities: LayerRenderer.Capabilities, configuration: inout LayerRenderer.Configuration) {
        configuration.colorFormat = .bgra8Unorm_srgb
        configuration.depthFormat = .depth32Float
        VxrApplicationHost.configureHover(capabilities: capabilities, configuration: &configuration)
        configuration.isFoveationEnabled = false
        configuration.layout = capabilities.supportedLayouts(options: []).contains(.dedicated) ? .dedicated : .layered
    }
}

@main
struct GltfXrApp: App {
    @State private var host: VxrApplicationHost = {
        let host = VxrApplicationHost()
        host.immersionStyle = .mixed
        return host
    }()
    @State private var running = false
    @Environment(\.scenePhase) private var scenePhase
    @Environment(\.dismissImmersiveSpace) private var dismissSpace

    // End the process after host cleanup until in-process relaunch is reliable
    @MainActor
    private func finish() async {
        await host.finish(dismissSpace)
        _exit(0)
    }

    var body: some Scene {
        ImmersiveSpace(id: "gltfxr") {
            CompositorLayer(configuration: ModelConfiguration()) { @MainActor layer in
                guard !running else { return }
                guard let resources = Bundle.main.resourcePath,
                      FileManager.default.changeCurrentDirectoryPath(resources) else {
                    print("Couldn't find sample resources")
                    Task { await finish() }
                    return
                }

                host.setActive(scenePhase == .active)
                let result = host.attach(layer)
                guard result == 0 else {
                    print("Couldn't attach OpenXR: \(result)")
                    Task { await finish() }
                    return
                }

                running = true
                let renderThread = Thread {
                    layer.waitUntilRunning()
                    let result = RunGltfXr({ context in
                        guard let context else { return false }
                        return VxrApplicationHost.poll(Unmanaged<LayerRenderer>.fromOpaque(context).takeUnretainedValue())
                    }, Unmanaged.passUnretained(layer).toOpaque())

                    Task { @MainActor in
                        running = false
                        print("OpenXR client ended: \(result)")
                        await finish()
                    }
                }

                // Keep the render loop responsive to compositor deadlines
                renderThread.name = "gltfxr render"
                renderThread.qualityOfService = .userInteractive
                renderThread.start()
            }
            .onImmersionChange { _, context in host.reportImmersion(context.amount) }
        }
        .immersionStyle(selection: $host.immersionStyle, in: .full, .mixed)
        .onChange(of: scenePhase) { _, phase in host.setActive(phase == .active) }
    }
}
