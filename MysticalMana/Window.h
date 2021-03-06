#ifndef MMWINDOW_H
#define MMWINDOW_H

/*
* No graphics API needed
*/
#define GLFW_NO_API

#include <GLFW/glfw3.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "UserInputEvents.h"
#include "GlfwWindowDestroyer.h"

namespace MysticalMana
{
	class Window
	{
		public:
			Window(int width_in, int height_in, std::string title_in);
			~Window();
			bool Poll();
			GLFWwindow* GetUnderlyingWindow();
			std::unordered_map<UserInputEvents, bool> UserInputCheck();
			double GetCurrentTime();

		private:
			int m_width_;
			int m_height_;
			std::string m_title_;
			std::unique_ptr<GLFWwindow, GlfwWindowDestroyer> m_glfw_window_;
	};
}
#endif