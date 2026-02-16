#include "Logicarium.hpp"

namespace Logicarium {
void Logicarium::glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

Logicarium::Logicarium() {}

int Logicarium::Mainloop() {
  // Setup window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

  // GL 3.0 + GLSL 130
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  // Native decorations enabled (Standard Window)
  // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

  // Create window with graphics context
  GLFWwindow *window = glfwCreateWindow(1280, 720, "Logicarium", NULL, NULL);
  if (window == NULL)
    return 1;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Setup contexts
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;

  // Setup style
  // Setup style
  // Modern Dark Theme
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.FrameRounding = 5.0f;
  style.PopupRounding = 5.0f;
  style.ScrollbarRounding = 5.0f;
  style.GrabRounding = 5.0f;
  style.FramePadding = ImVec2(10, 6);
  style.WindowPadding = ImVec2(10, 10);
  style.ItemSpacing = ImVec2(8, 8);

  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.32f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.30f, 0.32f, 1.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  ImVec4 clear_color = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

  NodeEditor nodeEditor;

  while (!glfwWindowShouldClose(window)) {
    // Poll and handle events (inputs, window resize, etc.)
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Standard Window Rendering
    // No custom dragging/resizing logic needed for native window

    nodeEditor.Redraw();

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
} // namespace Logicarium
