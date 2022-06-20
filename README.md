# Zombie Shooter AI


This is an AI designed to play a top-down zombie shooter game with limited information of its environment, according to a multi-layered behavior tree.

It was developed during the summer of 2021 as an exam assignment for the "Gameplay Programming" course at Howest University (part of the Game Development major). As such, the game itself, as well as the C++ framework used to developed the AI, were not programmed by me - being instead offered as part of the course's materials.

This "base" was offered to us since the actual challenge and goal of this project was to develop a skillful bot using a very limited scope of environmental information. All that the bot can access is which objects/enemies enter its short view-cone - and it can only access the target's information WHILE inside the cone. An example of the type of challenges that come with this limitation is, in an hypothetical scenario, if the bot is out of bullets and fleeing an enemy, as soon as direct eye-contact is broken, the bot can no longer know the zombie's position - it has to instead guess the direction in which it should run according to the zombie's last percieved position, velocity and direction.

That being said, the AI's not only capable of fleeing - it also collects resources, manages its personal inventory, uses healing items, aims and shoots at enemies (keeping track of ammunition), seeks refuge in closed spaces such as houses, and acts accordingly when faced with an environmental hazard (aka, "killzones").


<br />
<br />


[![Showcase](https://github.com/MiguelCPereira/ZombieShooterAI/blob/main/Showcase%20Thumbnail.png)](http://www.youtube.com/watch?v=wSMjNpLahuo "Zombie Shooter AI Showcase")
