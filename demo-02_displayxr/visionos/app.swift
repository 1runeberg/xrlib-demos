// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
@preconcurrency import CompositorServices
import SwiftUI
import Metal
import VisionOpenXR

struct DisplayConfiguration: CompositorLayerConfiguration {
    func makeConfiguration(capabilities: LayerRenderer.Capabilities, configuration: inout LayerRenderer.Configuration) {
        configuration.colorFormat = .bgra8Unorm_srgb
        configuration.depthFormat = .depth32Float
        configuration.isFoveationEnabled = false
        configuration.layout = capabilities.supportedLayouts(options: []).contains(.dedicated) ? .dedicated : .layered
    }
}

@main
struct DisplayXrApp: App {
    @State private var host: VxrImmersionHost = {
        let host = VxrImmersionHost()
        host.immersionStyle = .full
        return host
    }()
    @State private var status = "Basic XR rendering using xrlib and xrvk"
    @State private var running = false

    var body: some Scene {
        WindowGroup {
            Launcher(status: status, running: running)
                .padding(32).frame(width: 520)
        }
        ImmersiveSpace(id: "displayxr") {
            CompositorLayer(configuration: DisplayConfiguration()) { @MainActor layer in
                guard !running else { return }

                let result = host.attach(layer)
                guard result == 0 else {
                    status = "Couldn't attach OpenXR: \(result)"
                    return
                }

                guard let resources = Bundle.main.resourcePath,
                      FileManager.default.changeCurrentDirectoryPath(resources) else {
                    vxrCompositorDetach()
                    status = "Couldn't find sample resources"
                    return
                }

                running = true
                status = "Floor and axis indicators"

                Thread {
                    layer.waitUntilRunning()
                    let result = RunDisplayXr({ context in
                        guard let context else { return false }
                        let layer = Unmanaged<LayerRenderer>.fromOpaque(context).takeUnretainedValue()

                        if layer.state == .paused {
                            vxrCompositorSetActive(0)
                            layer.waitUntilRunning()
                        }

                        if layer.state == .invalidated {
                            vxrCompositorSetActive(0)
                            return false
                        }

                        vxrCompositorSetActive(1)
                        return true
                    }, Unmanaged.passUnretained(layer).toOpaque())

                    Task { @MainActor in
                        vxrCompositorSetActive(0)
                        vxrCompositorDetach()
                        running = false
                        status = result == 0 ? "Sample closed" : "Sample ended: \(result)"
                    }
                }.start()
            }
            .onImmersionChange { _, context in host.reportImmersion(context.amount) }
        }.immersionStyle(selection: $host.immersionStyle, in: .full)
    }
}

struct Launcher: View {
    let status: String
    let running: Bool
    @Environment(\.openImmersiveSpace) private var openSpace
    @Environment(\.dismissImmersiveSpace) private var closeSpace

    var body: some View {
        VStack(spacing: 20) {
            Text("displayxr").font(.largeTitle)
            Text(status)
            Button("Open sample") { Task { await openSpace(id: "displayxr") } }.disabled(running)
            Button("Close sample") { Task { await closeSpace() } }.disabled(!running)
        }
    }
}
