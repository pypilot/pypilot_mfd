$fn=60;

box = [40, 5.8, 54];
thickness=2.2;

difference() {
    union() {
        translate([-box[0]/2-thickness*3,-box[1]/2-thickness,-box[2]/2+thickness])
        minkowski() {
            cube([box[0]+thickness*6, thickness, box[2]-2*thickness]);
            rotate([90,0,0])
            cylinder(r=thickness, h=.1);
        }
        minkowski() {
        cube([box[0], box[1], box[2]-thickness], center=true);
            sphere(thickness);
        }
    }
    translate([0,0,thickness])
    cube(box, center=true);
    translate([box[0]/2+thickness-.2,-thickness/2 , box[2]/5])
    rotate([0,90,0])
    linear_extrude(.6)
        text("pypilot", size=4.5);
    
    translate([0,0,-box[2]/3])
    rotate([0,90,0])
    cylinder(r=3, h=22.5);
    
    holes();
    mirror([0 ,0,1])
    holes();
}

module holes()
{
        hole();
mirror()
    hole();
}

module hole()
{
        translate([box[0]/2+2.5*thickness,0,-box[2]/3])
    rotate([90,0,0])
    cylinder(r=2, h=30);
}