"""autogenerated by genpy from PhidgetServo/ServoPosition.msg. Do not edit."""
import sys
python3 = True if sys.hexversion > 0x03000000 else False
import genpy
import struct


class ServoPosition(genpy.Message):
  _md5sum = "04e7b072b763b2e3ee692e3faf7e9c94"
  _type = "PhidgetServo/ServoPosition"
  _has_header = False #flag to mark the presence of a Header object
  _full_text = """#Deprecated. Prefer to use the one in corobot_msgs package, which is exactly the same

#
# Morgan Cormier <mcormier@coroware.com>
#
# Message used to set or get the position of a servo motor connected to a Phidget servo controller.
#
# Index is the index of the servo motor connected to the phidget device. 
# The maximum value of index depends on the Phidget device and how many connections it accepts

int8 index

# position is the angle in degree set for the motor selected with the index value. 
# position has a minimum and maximum value possible, that can be retrieve through two services
float32 position

"""
  __slots__ = ['index','position']
  _slot_types = ['int8','float32']

  def __init__(self, *args, **kwds):
    """
    Constructor. Any message fields that are implicitly/explicitly
    set to None will be assigned a default value. The recommend
    use is keyword arguments as this is more robust to future message
    changes.  You cannot mix in-order arguments and keyword arguments.

    The available fields are:
       index,position

    :param args: complete set of field values, in .msg order
    :param kwds: use keyword arguments corresponding to message field names
    to set specific fields.
    """
    if args or kwds:
      super(ServoPosition, self).__init__(*args, **kwds)
      #message fields cannot be None, assign default values for those that are
      if self.index is None:
        self.index = 0
      if self.position is None:
        self.position = 0.
    else:
      self.index = 0
      self.position = 0.

  def _get_types(self):
    """
    internal API method
    """
    return self._slot_types

  def serialize(self, buff):
    """
    serialize message into buffer
    :param buff: buffer, ``StringIO``
    """
    try:
      _x = self
      buff.write(_struct_bf.pack(_x.index, _x.position))
    except struct.error as se: self._check_types(se)
    except TypeError as te: self._check_types(te)

  def deserialize(self, str):
    """
    unpack serialized message in str into this message instance
    :param str: byte array of serialized message, ``str``
    """
    try:
      end = 0
      _x = self
      start = end
      end += 5
      (_x.index, _x.position,) = _struct_bf.unpack(str[start:end])
      return self
    except struct.error as e:
      raise genpy.DeserializationError(e) #most likely buffer underfill


  def serialize_numpy(self, buff, numpy):
    """
    serialize message with numpy array types into buffer
    :param buff: buffer, ``StringIO``
    :param numpy: numpy python module
    """
    try:
      _x = self
      buff.write(_struct_bf.pack(_x.index, _x.position))
    except struct.error as se: self._check_types(se)
    except TypeError as te: self._check_types(te)

  def deserialize_numpy(self, str, numpy):
    """
    unpack serialized message in str into this message instance using numpy for array types
    :param str: byte array of serialized message, ``str``
    :param numpy: numpy python module
    """
    try:
      end = 0
      _x = self
      start = end
      end += 5
      (_x.index, _x.position,) = _struct_bf.unpack(str[start:end])
      return self
    except struct.error as e:
      raise genpy.DeserializationError(e) #most likely buffer underfill

_struct_I = genpy.struct_I
_struct_bf = struct.Struct("<bf")