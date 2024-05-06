# SQLITE-MC-UUID
An sqlite extension that adds functions for looking up minecraft accounts by username or uuid.

All api calls are cached and considered valid for 1 hour. Naturally this cache is implemented as a new table called `mc_profile_cache`.

### UUID -> USERNAME
```
sqlite> select username('0f75a81d-70e5-43c5-b892-f33c524284f2');
popbob
```

### USERNAME -> UUID
```
sqlite> select mc_uuid('popbob');
0f75a81d-70e5-43c5-b892-f33c524284f2
```
