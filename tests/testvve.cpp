
#include <iostream>
#include <utility>

#include "VHInclude.h"
#include "VEInclude.h"


template<vve::ArchitectureType ATYPE>
class MyGUI : public vve::System<ATYPE> {

    using vve::System<ATYPE>::m_engine;
	using typename vve::System<ATYPE>::Message;
	using typename vve::System<ATYPE>::MsgKeyDown;
	using typename vve::System<ATYPE>::MsgKeyUp;
	using typename vve::System<ATYPE>::MsgKeyRepeat;

public:
    MyGUI( vve::Engine<ATYPE>& engine ) : vve::System<ATYPE>("MyGUI", engine ) {

		m_engine.RegisterCallback( { 
			  {this, -10000, "RECORD_NEXT_FRAME", [this](Message message){this->OnRecordNextFrame(message);} }
			, {this,      0, "SDL_KEY_DOWN", [this](Message message){this->OnKeyDown(message);} }
			, {this,      0, "SDL_KEY_REPEAT", [this](Message message){this->OnKeyRepeat(message);} }
			, {this,      0, "SDL_KEY_UP", [this](Message message){this->OnKeyUp(message);} }
		} );
    };
    
    ~MyGUI() {};

    float clear_color[3]{ 0.45f, 0.55f, 0.60f};

    void OnRecordNextFrame(Message message) {
      
        static bool show_demo_window = false;
        static bool show_another_window = false;

        if( m_engine.GetWindow("VVE Window")->GetIsMinimized()) {
			return;
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
    }


    void OnKeyDown(Message message) {
        std::cout << "Key down: " << message.template GetData<MsgKeyDown>().m_key << std::endl;
    }

    void OnKeyUp(Message message) {
        std::cout << "Key up: " << message.template GetData<MsgKeyUp>().m_key << std::endl;
    }
    void OnKeyRepeat(Message message) {
        std::cout << "Key repeat: " << message.template GetData<MsgKeyRepeat>().m_key << std::endl;
    }

private:
};


template<vve::ArchitectureType ATYPE>
class MyEngine : public vve::Engine<ATYPE> {


	public:
		MyEngine() : vve::Engine<ATYPE>("MyEngine") {};
		~MyEngine() {};

	protected:
		
		void CreateGUI() override {}

		void LoadLevel(std::string levelName) {
			std::cout << "Loading level: " << levelName << std::endl;
		}

		MyGUI<ATYPE> mygui{this};

};



int main() {


    vve::Engine<vve::ENGINETYPE_SEQUENTIAL> engine("My Engine") ;

	MyGUI<vve::ENGINETYPE_SEQUENTIAL> mygui{engine};

	engine.Init();
	engine.PrintCallbacks();

    engine.Run();

    return 0;
}

