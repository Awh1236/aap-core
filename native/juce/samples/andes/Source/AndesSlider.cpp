/*
    Andes - synthesiser plugin based on Perlin noise
    Copyright (C) 2017  Artem Popov <art@artfwo.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../JuceLibraryCode/JuceHeader.h"
#include "AndesSlider.h"

//==============================================================================
AndesSlider::AndesSlider()
{
}

AndesSlider::~AndesSlider()
{
}

void AndesSlider::setGetTextFromValueFunc(std::function<String(double)> getTextFromValueFunc)
{
    this->getTextFromValueFunc = getTextFromValueFunc;
}

String AndesSlider::getTextFromValue(double value)
{
    if (getTextFromValueFunc)
    {
        return getTextFromValueFunc(value);
    }
    return Slider::getTextFromValue(value);
}
