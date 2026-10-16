extends Vehicle

@onready var motorcycle = $"Container/Model/motorcycle"
@onready var fork = $"Container/Model/motorcycle/body/fork"

@onready var wheel_front = $"Container/Model/motorcycle/wheel-front"
@onready var wheel_back = $"Container/Model/motorcycle/wheel-back"

func _ready():
	
	vehicle_body = $Container/Model/motorcycle/body

# Overwrite functions from base vehicle script

func effect_body(delta):
	
	var target_lean = -input.x / 5 * linear_speed 
	calculated_lean = lerp_angle(calculated_lean, target_lean, delta * 5)
	
	# Apply leaning when doing corners
	
	if motorcycle != null:
		motorcycle.rotation.z = lerp_angle(motorcycle.rotation.z, input.x * linear_speed , delta * 3)
		vehicle_body.rotation.x = lerp_angle(vehicle_body.rotation.x, -(linear_speed - acceleration) / 6, delta * 10)

func effect_wheels(delta):
	
	# Rotate wheels based on acceleration
	
	for wheel in [wheel_front, wheel_back]:
		if wheel != null:
			wheel.rotation.x += acceleration

	# Handle steering
	
	if wheel_front != null:
		fork.rotation.y = lerp_angle(fork.rotation.y, -input.x / 1.5, delta * 5)
		wheel_front.rotation.y = lerp_angle(wheel_front.rotation.y, -input.x / 1.5, delta * 10)
