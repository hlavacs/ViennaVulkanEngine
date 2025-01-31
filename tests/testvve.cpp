
#include <iostream>
#include <utility>

#include "VHInclude.h"
#include "VEInclude.h"


class MyGUI : public vve::System {

public:
    MyGUI( vve::Engine& engine ) : vve::System("MyGUI", engine ) {

		m_engine.RegisterCallback( { 
			//{this,      0, "LOAD_LEVEL", [this](Message& message){ return OnLoadLevel(message);} },
			{this, -10000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,      0, "SDL_KEY_DOWN", [this](Message& message){ return OnKeyDown(message);} },
			{this,      0, "SDL_KEY_REPEAT", [this](Message& message){ return OnKeyRepeat(message);} },
			{this,      0, "SDL_KEY_UP", [this](Message& message){ return OnKeyUp(message);} }
		} );
    };
    
    ~MyGUI() {};

	bool OnLoadLevel( Message message ) {
		auto msg = message.template GetData<vve::System::MsgLoadLevel>();	
		std::cout << "Loading level: " << msg.m_level << std::endl;

		auto handle = m_registry.Insert(
								vve::Name{msg.m_level},
								vve::Position{ { 0.0f, 0.0f, 0.0f } }, 
								vve::Rotation{vve::Rotation{glm::rotate(glm::mat4(1.0f), 3.14152f, glm::vec3(0.0f,0.0f,1.0f))}}, 
								vve::Scale{ { 1000.0f, 1000.0f, 1000.0f } } );

		m_engine.SendMessage( 
					MsgSceneLoad{
						this, 
						nullptr, 
						vve::ObjectHandle{handle}, 
						vve::ParentHandle{}, 
						vve::Name{"assets\\test\\plane\\plane_t_n_s.obj"} });

		return false;
	};

    float clear_color[3]{ 0.45f, 0.55f, 0.60f};

    bool OnRecordNextFrame(Message message) {
      
        static bool show_demo_window = false;
        static bool show_another_window = false;

        if( m_engine.GetWindow("VVE Window")->GetIsMinimized()) {
			return false;
		}

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / m_io->Framerate, m_io->Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }
		
        m_engine.GetWindow("VVE Window")->SetClearColor( glm::vec4{ clear_color[0], clear_color[1], clear_color[2], 1.0f} );
		return false;
    }


    bool OnKeyDown(Message message) {
        //std::cout << "Key down: " << message.template GetData<MsgKeyDown>().m_key << std::endl;
		return false;
    }

    bool OnKeyUp(Message message) {
        //std::cout << "Key up: " << message.template GetData<MsgKeyUp>().m_key << std::endl;
		return false;
    }

    bool OnKeyRepeat(Message message) {
        //std::cout << "Key repeat: " << message.template GetData<MsgKeyRepeat>().m_key << std::endl;
		return false;
    }

	private:
};



int main() {

    vve::Engine engine("My Engine") ;

	MyGUI mygui{engine};

	engine.Init();
	engine.PrintCallbacks();

    engine.Run();

    return 0;
}

