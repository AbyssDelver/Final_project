#include <SFML/Graphics.hpp>
#include <TGUI/TGUI.hpp>
#include <algorithm>  //for for_each
#include <random>     //for marsenne twister and uniform

#include "boid.hpp"
#include "constants.hpp"
#include "gui.hpp"
#include "point.hpp"
#include "quadtree.hpp"
#include "sfml.hpp"
#include "statistics.hpp"

namespace boids {

// random uniform distriubution of values, for randomly
// generating position of boids.
// Param 1: minimum generated value
// Param 2: maximum generated value
double uniform(double a, double b, std::mt19937& mt) {
  std::uniform_real_distribution<double> unif{a, b};
  return unif(mt);
}

// template, to take both predators and boid types
template <class T>
void initialize_birds(std::vector<T>& bird_vec, sf::VertexArray& vertices,
                      double swarm_n, sf::Color bird_color, std::mt19937& mt) {
  bird_vec.clear();
  vertices.clear();

  for (int i = 0; i < swarm_n; ++i) {
    // initializes boid within a margin from screen border and control panel
    auto boid_position = boids::Point{
        boids::uniform(constants::margin_size + constants::controls_width,
                       constants::window_width - constants::margin_size, mt),
        boids::uniform(constants::margin_size,
                       constants::window_height - constants::margin_size, mt)};
    auto boid_velocity =
        boids::Point{boids::uniform(constants::min_rand_velocity,
                                    constants::max_rand_velocity, mt),
                     boids::uniform(constants::min_rand_velocity,
                                    constants::max_rand_velocity, mt)};
    bird_vec.push_back(T{boid_position, boid_velocity});

    // append each vertex to the boid_vertex array
    sf::Vertex v1(sf::Vector2f(boid_position.x(), boid_position.y()),
                  bird_color);
    sf::Vertex v2(sf::Vector2f(boid_position.x(), boid_position.y()),
                  bird_color);
    sf::Vertex v3(sf::Vector2f(boid_position.x(), boid_position.y()),
                  bird_color);

    vertices.append(v1);
    vertices.append(v2);
    vertices.append(v3);
  }
}
}  // namespace boids

int main() {
  std::vector<boids::Boid> boid_vector;
  std::vector<boids::Predator> predator_vector;

  // array of vertices of triangle of a boid.
  // for each boid three vertices
  sf::VertexArray boid_vertex{sf::Triangles};

  // array of vertices of triangle of a predator
  // for each predator three vertices
  sf::VertexArray predator_vertex{sf::Triangles};

  // seeded marsenne twister engine, for random positions/velocities of boids
  std::mt19937 mt{std::random_device{}()};

  // makes the window and specifies it's size and title
  sf::RenderWindow window;
  window.create(
      sf::VideoMode(constants::window_width, constants::window_height),
      "boids!", sf::Style::Default);

  // lock framerate to 60 fps
  window.setFramerateLimit(60);

  tgui::GuiSFML gui{window};

  // vectores to store distances and speed of the boids
  std::vector<double> distances;
  std::vector<double> speeds;
  // creating the label to display all the stats
  tgui::Label::Ptr stats_label = tgui::Label::create();
  stats_label->getRenderer()->setTextColor(sf::Color::Black);
  stats_label->getRenderer()->setBackgroundColor(tgui::Color::White);
  gui.add(stats_label);

  // clock for fps calculation
  sf::Clock clock;

  // booleans for gui buttons
  bool display_tree{false};
  bool display_range{false};
  bool display_separation_range{false};
  bool display_prey_range{false};

  // panel object, it manages the tgui sliders, labels and buttons
  boids::Panel panel(constants::widget_width, constants::widget_height,
                     constants::gui_element_distance,
                     constants::first_element_x_position,
                     constants::first_element_y_position);

  boids::initialize_panel(gui, panel, display_tree, display_range,
                          display_separation_range, display_prey_range);

  // bool for tracking if moused is pressed, for boid repulsion
  bool is_mouse_pressed{false};

  // declaring boid parameters
  double separation_coefficent{};
  double cohesion_coefficent{};
  double alignment_coefficent{};
  double range{};
  double separation_range{};
  double prey_range{};
  double predator_range{};

  // initialize with absurd number so it automatically initializes boids
  int boid_number{-1};
  int predator_number{};

  // SFML loop. After each loop the window is updated
  while (window.isOpen()) {
    // fps calculation
    auto current_time = clock.restart().asSeconds();
    double fps = 1. / (current_time);

    for (const auto& boid : boid_vector) {
      distances.push_back(boid.pos().distance());
      speeds.push_back(boid.vel().distance());
    }
    double mean_distance = boids::calculate_mean_distance(boid_vector);
    double distance_stddev =
        boids::calculate_standard_deviation(distances, mean_distance);
    double mean_speed = boids::calculate_mean_speed(boid_vector);
    double speed_stddev =
        boids::calculate_standard_deviation(speeds, mean_speed);
    stats_label->setText(
        "Mean distance: " + std::to_string(mean_distance) +
        "\nStd Dev of distances: " + std::to_string(distance_stddev) +
        "\nMean Velocity: " + std::to_string(mean_speed) +
        "\nStd Dev of velocities: " + std::to_string(speed_stddev));

    float label_width = stats_label->getSize().x;
    float x_offset = window.getSize().x - label_width - 10;
    float y_offset = 10;
    stats_label->setPosition(x_offset, y_offset);

    sf::Event event;

    // if some input is given:
    while (window.pollEvent(event)) {
      gui.handleEvent(event);
      if (event.type == sf::Event::Closed) window.close();

      // if gui.handleEvent(event) is true (ex. a button is pressed) no
      // repulsion occours
      if (event.type == sf::Event::MouseButtonPressed &&
          !gui.handleEvent(event)) {
        is_mouse_pressed = true;
      }

      if (event.type == sf::Event::MouseButtonReleased) {
        is_mouse_pressed = false;
      }
    }

    // updating game from GUI  /////////////////////////////////////////////////
    // update the value of boid parameters based on the slider values
    boids::update_from_panel(panel, fps, cohesion_coefficent,
                             alignment_coefficent, separation_coefficent, range,
                             separation_range, prey_range);
    predator_range = constants::prey_to_predator_coeff * prey_range;

    // if the value of the slider is changed, change number of boids
    if (boids::update_boid_number(boid_number, panel)) {
      initialize_birds(boid_vector, boid_vertex, boid_number,
                       constants::boid_color, mt);
    }

    // if the value of the slider is changed, change number of predators
    if (boids::update_predator_number(predator_number, panel)) {
      initialize_birds(predator_vector, predator_vertex, predator_number,
                       constants::predator_color, mt);
    }

    // updating positions of boids/predators  //////////////////////////////////

    // quad tree object, partitions space improving performance
    boids::Quad_tree tree{
        constants::cell_capacity,
        boids::Rectangle{
            (constants::window_width + constants::controls_width) / 2.,
            constants::window_height / 2.,
            (constants::window_width - constants::controls_width) / 2.,
            constants::window_height / 2.}};

    for (auto& boid : boid_vector) {
      tree.insert(boid);
    }

    // handles boid/predator repulsion
    if (is_mouse_pressed) {
      boids::Point mouse_position(sf::Mouse::getPosition(window).x,
                                  sf::Mouse::getPosition(window).y);
      for (auto& boid : boid_vector) {
        if ((boid.pos() - mouse_position).distance() < constants::repel_range)
          boid.repel(mouse_position, constants::repel_range,
                     constants::repel_coefficent);
      }

      for (auto& predator : predator_vector) {
        if ((predator.pos() - mouse_position).distance() <
            constants::repel_range)
          predator.repel(mouse_position, constants::repel_range,
                         constants::repel_coefficent);
      }
    }

    // updates the predator positions
    for (int i = 0; i != static_cast<int>(predator_vector.size()); ++i) {
      predator_vector[i].update(constants::delta_t_predator, predator_range,
                                boid_vector);
      boids::vertex_update(predator_vertex, predator_vector[i], i,
                           constants::predator_size);
    }

    // updates the boid positions
    for (int i = 0; i != static_cast<int>(boid_vector.size()); ++i) {
      std::vector<const boids::Boid*> in_range;

      // tree builds the vector of in range boids
      tree.query(range, boid_vector[i], in_range);

      boid_vector[i].update(constants::delta_t_boid, in_range, separation_range,
                            separation_coefficent, cohesion_coefficent,
                            alignment_coefficent);

      // moves away boid from in range predators
      for_each(predator_vector.begin(), predator_vector.end(),
               [&, i](boids::Predator& predator) {
                 boid_vector[i].repel(predator.pos(), prey_range,
                                      constants::predator_avoidance_coeff);
               });

      boids::vertex_update(boid_vertex, boid_vector[i], i,
                           constants::boid_size);
    }

    // drawing objects to window ///////////////////////////////////////////////

    // makes the window return black
    window.clear(sf::Color::Black);

    window.draw(boid_vertex);
    window.draw(predator_vertex);

    // if the show cells button is pressed the tree object is displayed
    if (display_tree) tree.display(window);

    // if corresponding button is pressed, displays the ranges of the first boid
    // in the vector
    boids::display_ranges(range, separation_range, prey_range, display_range,
                          display_separation_range, display_prey_range,
                          boid_vector, window);
    gui.draw();
    window.display();
  }
}