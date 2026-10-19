#include "weapon_wheel.h"
#include <cassert>
#include <cstdio>
int main(){
    slice::Street inventory;
    assert(slice::WeaponWheel::button(233,140));assert(!slice::WeaponWheel::button(210,140));assert(!slice::WeaponWheel::button(233,160));
    assert(slice::WeaponWheel::choice(128,36,inventory)==0);
    assert(slice::WeaponWheel::choice(169,48,inventory)==-1);
    assert(slice::WeaponWheel::choice(128,88,inventory)==-2);
    assert(slice::WeaponWheel::choice(240,20,inventory)==-1);
    assert(!inventory.selectWeapon(true));
    inventory.grantPistol(7,48);assert(inventory.armed && inventory.hasPistol());
    assert(slice::WeaponWheel::choice(169,48,inventory)==1);
    assert(inventory.selectWeapon(false) && !inventory.armed && inventory.hasPistol());
    assert(inventory.ammo==7 && inventory.reserve==48);
    assert(inventory.selectWeapon(true) && inventory.armed && inventory.ammo==7);
    inventory.ammo=inventory.reserve=0;assert(inventory.selectWeapon(false));assert(inventory.hasPistol());
    assert(inventory.selectWeapon(true) && inventory.ammo==0 && inventory.reserve==0);
    std::puts("Weapon wheel: touch hit regions, locked slots, cancellation, holster, ownership and ammunition preservation passed");
}
