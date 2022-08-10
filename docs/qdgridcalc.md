qdgridcalc can be used to calculate grid size for given projection area

Usage:

    Usage: qdgridcalc [options] projectionstring x_km y_km

Projection string is in [SmartMet Projection format](projection-descriptions-in-SmartMet.md)

Options:

* **-s**  
    if projection string contains grid size, print grid resolution
* **-f qdfile**  
    Print grid size of given data file

### Example

    qdgridcalc stereographic,10,90,60:-13.8,31.8,73.2,55.6 25 25
    Grid size would be: 232 x 172
    Grid size would be at center of bottom edge: 21.9274 x 21.8816 km
    Grid size would be at center:           24.9983 x 24.9402 km
    Grid size would be at center of top edge: 27.0825 x 26.9947 km

Here we calculate what would be the grid size for this projection and area, if we want 25x25km resolution.
