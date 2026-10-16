extends Node3D

@export_group("Properties")
@export var target: Vehicle

@onready var camera = $Camera

# Functions

func _physics_process(delta):
	
	# Ease position towards target vehicle position
	
	self.position = self.position.lerp(target.get_vehicle_position(), delta * 4)

	# Zoom camera based on the speed of the vehicle

	var speed_factor = clamp(abs(target.linear_speed), 0.0, 1.0)
	var target_z = remap(speed_factor, 0.0, 1.0, 10, 20)
	
	camera.position.z = lerp(camera.position.z, target_z, delta * 0.5)
