#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/component/component.hpp"  // for Checkbox, Renderer, Horizontal, Vertical, Input, Menu, Radiobox, ResizableSplitLeft, Tab, Toggle
#include "ftxui/component/component_base.hpp"     // for ComponentBase
#include "ftxui/component/component_options.hpp"  // for InputOption
#include "ftxui/component/event.hpp"              // for Event, Event::Custom
#include "ftxui/component/screen_interactive.hpp"  // for Component, ScreenInteractive
#include "ftxui/dom/elements.hpp"  // for text, operator|, color, bgcolor, filler, Element, size, vbox, flex, hbox, separator, graph, EQUAL, paragraph, hcenter, WIDTH, bold, window, border, vscroll_indicator, Elements, HEIGHT, hflow, frame, flex_grow, flexbox, gauge, paragraphAlignCenter, paragraphAlignJustify, paragraphAlignLeft, paragraphAlignRight, dim, spinner, Decorator, LESS_THAN, center, yflex, GREATER_THAN
#include "ftxui/screen/color.hpp"  // for Color, Color::Blue, Color::BlueLight, Color::RedLight, Color::Black, Color::Cyan, Color::CyanLight, Color::GrayDark, Color::GrayLight, Color::Green, Color::GreenLight, Color::Magenta, Color::MagentaLight, Color::Red, Color::White, Color::Yellow, Color::YellowLight, Color::Default
#include "ftxui/screen/terminal.hpp"  // for Size, Dimensions
#include "yaml-cpp/yaml.h"

using namespace ftxui;

Component Window(std::string title, Component component) {
  return Renderer(component, [component, title] {  //
    return window(text(title), component->Render()) | flex;
  });
}

// int source_selected;
// ftxui::Element generate_component_details(const std::string component_file)
// {
// //  auto yaml = YAML::LoadFile(component_file);
//   return hbox({
//     vbox({
//       text("Sources") | border,
//       Menu(&lighting_app_sources, &source_selected)
//     }),
//     vbox({
//       text("Defines") | border
//     })
//   });
// }

std::vector<std::string> lighting_app_sources = {
  "src/AppTask.cpp",
  "src/LightingManager.cpp",
  "src/ZclCallbacks.cpp",
  "src/main.cpp"
};

std::vector<std::string> component_names = {
    "Lighting app",
    "Matter",
    "EFR32 support for Matter",
    "OpenThread",
    "EFR32 support for OpenThread",
    "Pigweed",
    "NLIO",
    "NLAssert",
    "GCC for ARM",
    "QRCode",
    "Silicon Labs Gecko SDK"
};

std::vector<std::string> component_files = {
    "lighting-app.bob",
    "matter.bob",
    "EFR32 support for Matter",
    "OpenThread",
    "EFR32 support for OpenThread",
    "Pigweed",
    "NLIO",
    "NLAssert",
    "GCC for ARM",
    "QRCode",
    "Silicon Labs Gecko SDK"
};

std::vector<std::string> available_component_names = {
    "LwIP",
    "FreeRTOS",
    "CLI",
    "Wi-Fi",
    "BLE"
};

const std::string lighting_app_details = R"(An example showing the use of CHIP on the Silicon Labs EFR32.
Introduction

The EFR32 lighting example provides a baseline demonstration of a Light control
device, built using CHIP and the Silicon Labs gecko SDK. It can be controlled by
a Chip controller over Openthread network..

The EFR32 device can be commissioned over Bluetooth Low Energy where the device
and the Chip controller will exchange security information with the Rendez-vous
procedure. Thread Network credentials are then provided to the EFR32 device
which will then join the network.

The LCD on the Silabs WSTK shows a QR Code containing the needed commissioning
information for the BLE connection and starting the Rendez-vous procedure.

The lighting example is intended to serve both as a means to explore the
workings of CHIP as well as a template for creating real products based on the
Silicon Labs platform.)";

int main(int argc, const char* argv[]) {
  int shift = 0;
  auto screen = ScreenInteractive::Fullscreen();

    

  std::string component_configuration_text = "Component configuration goes here";

  int project_selected = 0;
  int available_selected = 0;
  MenuOption menu_option;
//   auto component_menu = Menu(&component_names, &selected, &menu_option);

  auto component_details = Renderer([&] { 
    Elements list;
    std::stringstream ss(lighting_app_details);
    std::string line;
    while(std::getline(ss, line,'\n')) {
      list.push_back(text(line));
   }
   return vbox(std::move(list)) | frame | border;
  //  return vbox({ paragraph(lighting_app_details) | border }); 
   });
  auto component_configuration_details = Renderer([&] { return vbox({ text(component_configuration_text) | border }); });

  int tab_index = 0;
  std::vector<std::string> component_tab_names = { "Details", "Configuration" };
  auto tab_selection = Toggle(&component_tab_names, &tab_index);
  auto tab_content = Container::Tab({
      component_details,
      component_configuration_details,
    }, &tab_index);


  auto component_container = Container::Horizontal({
      Container::Vertical({
          Window("Project Components", Menu(&component_names, &project_selected, &menu_option)),
          Window("Available Components", Menu(&available_component_names, &available_selected)),
      }),
      Container::Vertical({
        tab_selection,
        tab_content,
      }),
  });

//   auto component_renderer = Renderer(compiler_component, [&] {
//     auto component_window = window(text("Component"), component_menu->Render() | vscroll_indicator | frame);
    
//     auto component_details_window = window(text("Flags"), flags->Render() | vscroll_indicator | frame);
//     return vbox({
//                hbox({
//                    component_window,
//                    flags_win,
//                    filler(),
//                }) | size(HEIGHT, LESS_THAN, 8),
//                hflow(render_command()) | flex_grow,
//            }) |
//            flex_grow | border;
//   });

//   int main_tab_index = 0;
//   std::vector<std::string> main_tab_entries = {
//       "Components",
//   };
//   auto main_tab_selection = Toggle(&main_tab_entries, &main_tab_index);
//     auto main_container = Container::Vertical({
//       main_tab_selection,
//       component_container,
//     });

    bool refresh_ui_continue = true;
    std::thread refresh_ui([&] {
    while (refresh_ui_continue) {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(0.1s);
      shift++;
      screen.PostEvent(Event::Custom);
    }
  });


  auto main_renderer = Renderer(component_container, [&] {
    return vbox({
        vbox({
            text("Configurator") | bold | hcenter,
        }),
        component_container->Render(),
    });
  });

    screen.Loop(main_renderer);
  refresh_ui_continue = false;
  refresh_ui.join();

  return 0;
}



void process_project()
{

}