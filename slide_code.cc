#include <chrono>
#include <memory>

struct coordinates {
  int x;
};

namespace bad {

class Vehicle {
 public:
  Vehicle() = default;
  virtual ~Vehicle() = default;

  virtual std::unique_ptr<Vehicle> clone() const = 0;
  virtual void move(const coordinates&) = 0;

 protected:
  Vehicle(const Vehicle&) = default;
  Vehicle(Vehicle&&) = default;
  Vehicle& operator=(const Vehicle&) = default;
  Vehicle& operator=(Vehicle&&) = default;
};

class MotorBike : public Vehicle {
 public:
  std::unique_ptr<Vehicle> clone() const override {
    return std::make_unique<MotorBike>(*this);
  }
  void move(const coordinates&) override { /* do motorbike things */
  }
};

class Animal {};

class Horse : public Vehicle, public Animal {};

}  // namespace bad

namespace v1 {

class IVehicle {
 public:
  virtual IVehicle* clone() const = 0;
  virtual void move(const coordinates&) = 0;
  virtual ~IVehicle() = default;
};

class Vehicle {
 public:
  Vehicle() = default;
  explicit Vehicle(std::unique_ptr<IVehicle> body) : body_(std::move(body)) {}

  Vehicle(const Vehicle& x)
      : Vehicle(std::unique_ptr<IVehicle>{x.body_->clone()}) {}
  Vehicle(Vehicle&&) = default;

  Vehicle& operator=(const Vehicle& x) {
    auto tmp = x;
    *this = std::move(tmp);
    return *this;
  }
  Vehicle& operator=(Vehicle&&) = default;

  ~Vehicle() = default;

  void move(const coordinates& c) { body_->move(c); }

 private:
  std::unique_ptr<IVehicle> body_;
};

class MotorBike : public IVehicle {
 public:
  MotorBike* clone() const override { return new MotorBike(*this); }
  void move(const coordinates&) override {}
};

class IAnimal {
 public:
  virtual IAnimal* clone() const = 0;
  virtual ~IAnimal() = default;
};

class Animal {
 public:
  Animal() = default;
  explicit Animal(std::unique_ptr<IAnimal> body) : body_(std::move(body)) {}

  Animal(const Animal& x)
      : Animal(std::unique_ptr<IAnimal>{x.body_->clone()}) {}
  Animal(Animal&&) = default;

  Animal& operator=(const Animal& x) {
    auto tmp = x;
    *this = std::move(tmp);
    return *this;
  }
  Animal& operator=(Animal&&) = default;

  ~Animal() = default;

 private:
  std::unique_ptr<IAnimal> body_;
};

class Horse : public IVehicle, public IAnimal {
 public:
  void move(const coordinates&) override {}
  Horse* clone() const override { return new Horse(*this); }
};

}  // namespace v1

namespace v2 {

class Vehicle {
 public:
  Vehicle() = default;

  template <typename T>
  explicit Vehicle(T x) : body_(std::make_unique<model<T>>(std::move(x))) {}

  Vehicle(const Vehicle& x) : body_{x.body_->clone()} {}
  Vehicle(Vehicle&&) = default;

  Vehicle& operator=(const Vehicle& x) {
    auto tmp = x;
    *this = std::move(tmp);
    return *this;
  }
  Vehicle& operator=(Vehicle&&) = default;

  ~Vehicle() = default;

  void move(const coordinates& c) { body_->move(c); }

 private:
  struct concept {
    virtual std::unique_ptr<concept> clone() const = 0;
    virtual void move(const coordinates&) = 0;
    virtual ~concept() = default;
  };

  template <typename T>
  struct model final : concept {
    T adapted;

    model(T x) : adapted(std::move(x)) {}

    std::unique_ptr<concept> clone() const final {
      return std::make_unique<model>(*this);
    }
    void move(const coordinates& c) { adapted.move(c); }
  };

  std::unique_ptr<concept> body_;
};

class MotorBike {
 public:
  void move(const coordinates&) {}
};

}  // namespace v2

namespace v3 {

class Vehicle {
 public:
  Vehicle() = default;

  template <typename T>
  explicit Vehicle(T x)
      : body_{new T(std::move(x))}, vptr_{vtable_for_type<T>()} {}

  Vehicle(const Vehicle& x) : body_{x.vptr_->clone(x)}, vptr_{x.vptr_} {}
  Vehicle(Vehicle&& x)
      : body_(std::exchange(x.body_, nullptr)), vptr_(x.vptr_) {}

  Vehicle& operator=(const Vehicle& x) {
    auto tmp = x;
    *this = std::move(tmp);
    return *this;
  }
  Vehicle& operator=(Vehicle&& x) {
    body_ = std::exchange(x.body_, nullptr);
    vptr_ = x.vptr_;
    return *this;
  }

  ~Vehicle() { vptr_->destroy(*this); }

  void move(const coordinates& c) { vptr_->move(*this, c); }

  friend bool operator==(const Vehicle& x, const Vehicle& y) {
    return x.vptr_ == y.vptr_ && x.vptr_->equal(x, y);
  }

  friend bool operator!=(const Vehicle& x, const Vehicle& y) {
    return !(x == y);
  }

 private:
  struct vtable {
    virtual void* clone(const Vehicle&) const = 0;
    virtual void move(Vehicle&, const coordinates&) const = 0;
    virtual void destroy(Vehicle&) const = 0;
    virtual bool equal(const Vehicle& x, const Vehicle& y) const = 0;
  };

  static const vtable* null_vtable() {
    static constexpr struct _ final : vtable {
      void* clone(const Vehicle&) const final { return nullptr; }
      void move(Vehicle&, const coordinates&) const final {}
      void destroy(Vehicle&) const final {}
      bool equal(const Vehicle& x, const Vehicle& y) const final {
        return true;
      }
    } res;
    return &res;
  }

  template <typename T>
  T* cast() {
    return static_cast<T*>(body_);
  }

  template <typename T>
  const T* cast() const {
    return static_cast<const T*>(body_);
  }

  template <typename T>
  static const auto* vtable_for_type() {
    static constexpr struct _ final : vtable {
      void* clone(const Vehicle& self) const final {
        return new T{*self.cast<T>()};
      }
      void move(Vehicle& self, const coordinates& c) const final {
        self.cast<T>()->move(c);
      }
      void destroy(Vehicle& self) const final { delete self.cast<T>(); };
      bool equal(const Vehicle& x, const Vehicle& y) const final {
        return (*x.cast<T>()) == (*y.cast<T>());
      }
    } res;
    return &res;
  }

  void* body_ = nullptr;
  const vtable* vptr_ = null_vtable();
};

}  // namespace v3

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

namespace v1 {

TEST_CASE("testing copying horses") {
  Horse horse;

  Vehicle vehicle(std::make_unique<Horse>(horse));
  Animal animal(std::make_unique<Horse>(horse));
}

}  // namespace v1

namespace v2 {

TEST_CASE("motorbike") {
  MotorBike motor_bike;
  Vehicle v{motor_bike};
}

}  // namespace v2

struct Boat {
  void move(const coordinates& c) { pos = c.x; }

  friend bool operator==(const Boat& x, const Boat& y) {
    return x.pos == y.pos;
  }

  int pos = 0;
};

template <typename VehicleT>
void generic_test(){{VehicleT v;
}

{
  VehicleT v(Boat{});

  v = VehicleT(Boat{});
}

{
  VehicleT v1{Boat{}};

  VehicleT v2;
  v1 = v2;
}

{
  VehicleT v1{Boat{}};

  VehicleT v2{std::move(v1)};
}

{
  VehicleT v1{Boat{1}};
  VehicleT v2{Boat{2}};

  REQUIRE(v1 != v2);

  v1 = v2;
  REQUIRE(v1 == v2);

  v1.move(coordinates{5});
  REQUIRE(v1 != v2);
}

{
  VehicleT v1{Boat{}};
  VehicleT v2 = std::move(v1);
}

{
  VehicleT v1{Boat{}};
  VehicleT v2;
  v2 = std::move(v1);
}
}
;

namespace v3 {

TEST_CASE("V3") { generic_test<Vehicle>(); }

}  // namespace v3